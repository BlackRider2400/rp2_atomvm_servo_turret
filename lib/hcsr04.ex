defmodule HCSR04 do
  @trig 4
  @echo 5
  @timeout_us 30_000

  def init do
    :ok = :gpio.init(@trig)
    :ok = :gpio.init(@echo)
    :ok = :gpio.set_pin_mode(@trig, :output)
    :ok = :gpio.set_pin_mode(@echo, :input)
    :ok = :gpio.digital_write(@trig, 0)
    :timer.sleep(100)
    :ok
  end

  def measure_cm do
    :gpio.digital_write(@trig, 1)
    :timer.sleep(1)
    :gpio.digital_write(@trig, 0)

    case wait_echo(:high, @timeout_us) do
      {:ok, t1} ->
        case wait_echo(:low, @timeout_us) do
          {:ok, t2} ->
            us = t2 - t1
            cm = us / 58.0
            {:ok, cm}

          :timeout ->
            {:error, :timeout}
        end

      :timeout ->
        {:error, :timeout}
    end
  end

  defp wait_echo(level, timeout_us) do
    t_start = :erlang.system_time(:microsecond)
    wait_loop(level, t_start, timeout_us)
  end

  defp wait_loop(level, t_start, timeout_us) do
    current = :gpio.digital_read(@echo)
    t_now = :erlang.system_time(:microsecond)

    cond do
      current == level -> {:ok, t_now}
      t_now - t_start > timeout_us -> :timeout
      true -> wait_loop(level, t_start, timeout_us)
    end
  end
end
