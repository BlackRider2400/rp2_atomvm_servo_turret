defmodule TurretTest do
  use ExUnit.Case
  doctest Turret

  test "greets the world" do
    assert Turret.hello() == :world
  end
end
