defmodule PWM do
  @duty_min 1650
  @duty_mid 4900
  @duty_max 8150

  def init(pin, freq_hz \\ 50), do: :pwm.init(pin, freq_hz)
  def set_duty(pin, duty), do: :pwm.set_duty(pin, duty)
  def deinit(pin), do: :pwm.deinit(pin)

  def set_angle(pin, angle) when angle >= 0 and angle <= 180 do
    duty = @duty_min + round((@duty_max - @duty_min) * angle / 180)
    set_duty(pin, duty)
  end

  def center(pin), do: set_angle(pin, 90)
end
