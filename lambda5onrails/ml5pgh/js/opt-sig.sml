
signature JSOPT =
sig

  exception JSOpt of string
  
  (* assumes that this javascript code was generated by our code generator. *)
  val optimize : Javascript.Program.t -> Javascript.Program.t

end
