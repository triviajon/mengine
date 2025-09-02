import Std.Tactic
section Test

variable {α : Type}

variable (f : α → α) (g : α → α) (eq_fa_a : ∀ x : α, f x = x)
variable (a b : α)
