defmodule Turret do
  @pins %{
    servo_x: 2,
    servo_y: 1,
    joy_x: 26,
    joy_y: 27,
    btn: 28
  }

  @sweep %{
    step_x: 8,
    step_y: 2
  }

  @manual %{
    dead_zone: 10,
    speed: 3
  }

  @detect_cm 50.0
  @samples 2
  @loop_ms 15

  defp initial_state do
    %{
      mode: :manual,
      x: 90,
      y: 90,
      dir_x: 1,
      dir_y: 1,
      prev_btn: :high
    }
  end

  def start do
    IO.puts("[Turret] Start")
    init_hardware()
    center_servos()
    :timer.sleep(500)
    loop(initial_state())
  end

  defp init_hardware do
    :ok = PWM.init(@pins.servo_x, 50)
    :ok = PWM.init(@pins.servo_y, 50)
    :ok = ADC.init(@pins.joy_x)
    :ok = ADC.init(@pins.joy_y)
    :ok = :gpio.init(@pins.btn)
    :ok = :gpio.set_pin_mode(@pins.btn, :input)
    :ok = :gpio.set_pin_pull(@pins.btn, :up)
    :ok = HCSR04.init()
  end

  defp center_servos do
    PWM.center(@pins.servo_x)
    PWM.center(@pins.servo_y)
  end

  defp loop(state) do
    state
    |> read_button()
    |> tick()
    |> sleep_and_loop()
  end

  defp sleep_and_loop(state) do
    :timer.sleep(@loop_ms)
    loop(state)
  end

  defp read_button(%{prev_btn: prev} = state) do
    btn = :gpio.digital_read(@pins.btn)

    state =
      case {prev, btn} do
        {:high, :low} -> toggle_mode(state)
        _ -> state
      end

    %{state | prev_btn: btn}
  end

  defp toggle_mode(%{mode: :manual} = state) do
    IO.puts("[Turret] AUTO")
    %{state | mode: :auto, dir_x: 1, dir_y: 1}
  end

  defp toggle_mode(%{mode: :auto} = state) do
    IO.puts("[Turret] MANUAL")
    %{state | mode: :manual}
  end

  defp tick(%{mode: :manual} = state), do: manual_tick(state)
  defp tick(%{mode: :auto} = state), do: auto_tick(state)

  defp manual_tick(state) do
    {nx, ny} =
      {@pins.joy_x, @pins.joy_y}
      |> read_joystick()
      |> apply_deadzone(@manual.dead_zone)
      |> to_delta(@manual.speed)
      |> clamp_position(state.x, state.y)

    PWM.set_angle(@pins.servo_x, nx)
    PWM.set_angle(@pins.servo_y, ny)

    %{state | x: nx, y: ny}
  end

  defp read_joystick({pin_x, pin_y}) do
    {:ok, jx} = ADC.read_normalized(pin_x)
    {:ok, jy} = ADC.read_normalized(pin_y)
    {jx, jy}
  end

  defp apply_deadzone({jx, jy}, dz) do
    {
      if(abs(jx) > dz, do: jx, else: 0),
      if(abs(jy) > dz, do: jy, else: 0)
    }
  end

  defp to_delta({jx, jy}, speed) do
    {round(jx / 100 * speed), round(jy / 100 * speed)}
  end

  defp clamp_position({dx, dy}, ax, ay) do
    {clamp(ax + dx), clamp(ay + dy)}
  end

  defp auto_tick(state) do
    case detect(@samples) do
      {:ok, dist} -> on_target(state, dist)
      :none -> sweep(state)
    end
  end

  defp detect(0), do: :none

  defp detect(n) do
    case HCSR04.measure_cm() do
      {:ok, cm} when cm < @detect_cm -> {:ok, cm}
      _ -> detect(n - 1)
    end
  end

  defp on_target(state, dist) do
    IO.puts("[Turret] CEL #{round(dist)}cm  X=#{state.x} Y=#{state.y}")
    PWM.set_angle(@pins.servo_x, state.x)
    PWM.set_angle(@pins.servo_y, state.y)
    state
  end

  defp sweep(state) do
    {ny, dir_y, bounced?} = step_axis(state.y, state.dir_y, @sweep.step_y)

    {nx, dir_x} =
      if bounced? do
        {x, dx, _} = step_axis(state.x, state.dir_x, @sweep.step_x)
        {x, dx}
      else
        {state.x, state.dir_x}
      end

    PWM.set_angle(@pins.servo_x, nx)
    PWM.set_angle(@pins.servo_y, ny)

    %{state | x: nx, y: ny, dir_x: dir_x, dir_y: dir_y}
  end

  defp step_axis(value, dir, step) do
    next = value + dir * step

    cond do
      next >= 180 -> {180, -1, true}
      next <= 0 -> {0, 1, true}
      true -> {next, dir, false}
    end
  end

  defp clamp(angle), do: max(0, min(180, angle))
end
