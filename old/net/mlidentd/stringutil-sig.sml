
(* String utilities by Tom 7. *)

signature STRINGUTIL =
sig

  exception StringUtil of string

  (* table w sll

     formats a list sll (rows of columns of strings) into a
     table of maximum width w. Will word-wrap if necessary.
  *)

  val table : int -> string list list -> string

  val hardtable : int list -> string list list -> string

  (* pad n s
     pads s with spaces so it is n characters.
     for negative n, pad on the left instead of right. *)
  val pad : int -> string -> string

  val wrapto : int -> string -> string list

  val unformatted_table : string list list -> string

  val ucase : string -> string
  val lcase : string -> string

  (* read a whole file into a string.
     XXX: Does this really belong here? *)
  val readfile : string -> string

  val delimit : string -> string list -> string


  (* escape escapechar what input
     
     uses escapechar to escape any character
     in the charspec (see below) 'what', including
     escapechar itself. Tries to do so efficiently
     so that this is suited for large strings.

     *)
  val escape : char -> string -> string -> string

  (* harden f c len s

     for the super-conservative, makes a string safe for printing or
     logging.

     The function f passed in decides whether a punctuation character
     (non alpha-numeric) is considered "safe"; alpha-numeric (A-Z,
     a-z, 0-9) characters are always considered safe.

     The character c is used to escape all other characters (as two
     hex digits) -- so that character is always considered unsafe.
     It cannot be alphanumeric.
     
     the integer argument len gives a maximum length for the output.

     *)
  val harden : (char -> bool) -> char -> int -> string -> string

  val inlist : char list -> char -> bool
  val ischar : char -> char -> bool
  val isn'tchar : char -> char -> bool

  (* truncate n s
     if s is longer than n characters,
     return the first n characters,
     otherwise return the whole string. 
     *)
  val truncate : int -> string -> string

  (* filter f s
     Same as:
     implode o (List.filter f) o explode
  *)
  val filter : (char -> bool) -> string -> string

  (* "0123456789ABCDEF" *)
  val digits : string

  (* Convert a 32-bit word into its hex representation,
     big-endian or little-endian style. *)
  val wordtohex_be : Word.word -> string
  val wordtohex_le : Word.word -> string

  (* 80 to 0050 *)
  val word16tohex : Word.word -> string

  (* 42 to 2A *)
  val bytetohex : int -> string

  (* 0 = 0, A = 10, f = 15 *)
  val hexvalue : char -> int

  (* like List.all *)
  val all : (char -> bool) -> string -> bool

  (* pass in a regexp-style character list/range.
     "A-Za-z0-9" would match any alphanumeric, for instance.
     "A-Z!@" matches capital letters, !, and @.
     Use backslash to escape -, or put it at the beginning
     or end of the spec.
     If the string begins with ^, it is the negation of that
     set.
     *)
  val charspec : string -> char -> bool


  (* matchat n small big
     true if the string 'small' occurs within 'big'
     starting at character n. *)
  val matchat : int -> string -> string -> bool

  (* matchtail small big
     true if the string 'big' ends with the string 'small'. 
     *)
  val matchtail : string -> string -> bool

  (* matchtail small big
     true if the string 'big' starts with the string 'small'. 
     *)
  val matchhead : string -> string -> bool

  (* wcmatch w s 
     true if s matches the wildcard w.
     wildcards are DOS-style *.* things. 
     (? not supported currently.)
   *)
  val wcmatch : string -> string -> bool

  (* findat n small big
     find the first occurence of 'small' in 'big' that
     begins at character n or later. returns NONE if
     there is no such match. *)
  val findat : int -> string -> string -> int option
      
  (* find small big 
     same as findat 0 small big
   *)
  val find : string -> string -> int option

  (* same as above, but search from the end of the string
     towards the beginning. *)
  val rfindat : int -> string -> string -> int option
  val rfind : string -> string -> int option

  (* take string that potentially contains non-ascii bytes,
     and render it as a hexl-style hex dump. *)
  val hexdump : string -> string

  (* return n of character c *)
  val tabulate : int -> char -> string

end
