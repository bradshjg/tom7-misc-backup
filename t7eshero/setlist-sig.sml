signature SETLIST =
sig

  exception Setlist of string

  type songid
  type songinfo =
      { file : string,
        slowfactor : int,
        hard : int,
        fave : int,
        title : string,
        artist : string,
        year : string,
        id : songid }

  datatype background =
      BG_SOLID of SDL.color
    | BG_RANDOM of int (* frames between swaps *)

  type interlude =
    { background : background,
      graphic : SDL.surface option,
      robot : bool,
      (* Message at top and bottom of screen *)
      message1 : string option,
      message2 : string option }

  datatype command =
      WombOn
    | WombOff

  datatype showpart =
      Song of { song : songid,
                misses : bool,
                drumbank : int Vector.vector option,
                background : background
                (* ... ? *)
                }
    | Postmortem
    | Interlude of interlude
    | Wardrobe
    | Command of command
    (* XXX experimental *)
    | Minigame of { song : songid, misses: bool,
                    drumbank : int Vector.vector option,
                    background : background }

  type showinfo =
      { name : string,
        date : string,
        parts : showpart list }

  val allsongs : unit -> songinfo list
  val allshows : unit -> showinfo list

  val eq : songid * songid -> bool
  val cmp : songid * songid -> order
  val tostring : songid -> string
  val fromstring : string -> songid option

  val getsong : songid -> songinfo

  structure Map : ORD_MAP where type Key.ord_key = songid

end
