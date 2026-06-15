defmodule ADC do
  @center 2047
  @max 4095

  def init(pin), do: :adc.init(pin)
  def read(pin), do: :adc.read(pin)

  def read_normalized(pin) do
    {:ok, raw} = read(pin)
    normalized = (raw - @center) / (@max - @center) * 100
    clamped = max(-100, min(100, round(normalized)))
    {:ok, clamped}
  end
end
