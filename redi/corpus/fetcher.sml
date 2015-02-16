
structure Fetcher =
struct
  exception Fetcher of string

  val user_agent =
    "Mozilla/5.0 (Windows NT 6.1; WOW64) AppleWebKit/537.36 " ^
    "(KHTML, like Gecko) Chrome/40.0.2214.111 Safari/537.36"

  val referrer = "https://www.google.com/"

  (* Seconds to wait in-between fetches, no matter what. *)
  val throttle = ref 1

  fun okay_extension f =
    if StringUtil.matchtail ".jpg" f
    then SOME ".jpg"
    else if StringUtil.matchtail ".png" f
         then SOME ".png"
         else NONE

  fun get_image url hquery num =
    case okay_extension of
      SOME ext =>
        let
          val outfile = hquery ^ "-" ^ (Int.toString.num) ^ ext
          val cmd =
            "wget --no-check-certificate " ^
            "--quiet "
            "--user-agent \"" ^ user_agent ^ "\" " ^
            (* "--referer \"" ^ referrer ^ "\" " ^ *)
            "\"" ^ url ^ "\" " ^
            "--output-document " ^ outfile
          val ret = OS.Process.system cmd
        in
          OS.Process.isSuccess
          false
        end


  fun process_html hquery html =
    let
      val re =
        RE.findall ("href=\"http://www\\.google\\.com/imgres\\?imgurl=" ^ (* " *)
                    "([^&]+)" ^
                    "&amp;")
      val matches = List.mapPartial (fn f => StringUtil.urldecode (f 1)) (re html)
      val nmatches = length matches

      fun getone (s, i) =
        let in
          if get_image s hquery (i + 1)
          then print "."
          else print "!"
        end
    in
      print ("Downloading " ^ Int.toString nmatches ^ " match(es):\n"
             "[");
      ListUtil.appi getone matches;
      print "]\n";
      ()
    end

  fun fetch query =
    let
      (* Better just compile this one in, instead of wrangling
         with multi-escaping issues. *)

      val url =
        "https://www.google.com/search" ^
        "?q=" ^ StringUtil.urlencode query ^
        "&safe=off&tbm=isch&tbs=isz:lt,islt:2mp&nojs=1"

      val h = StringUtil.harden (fn c => c = #"-") #"_" 40 (CharVector.map (fn #" " => #"-"
                                                                             | c => c) query)
      val outfile = "fetcher/" ^ h ^ ".html"

      val cmd =
        "wget --no-check-certificate " ^
        "--quiet " ^
        "--user-agent \"" ^ user_agent ^ "\" " ^
        "--referer \"" ^ referrer ^ "\" " ^
        "\"" ^ url ^ "\" " ^
        "--output-document " ^ outfile

      val () = print ("Sleeping " ^ Int.toString (!throttle) ^ ".\n")
      val () = OS.Process.sleep (Time.fromSeconds (IntInf.fromInt (!throttle)))
      val ret = OS.Process.system cmd
    in
      if OS.Process.isSuccess ret
      then process_html h (StringUtil.readfile outfile)
      else
        print ("Failed. Query: " ^ query ^ "\n")
    end

end

val () = Params.main1 "(see source)" Fetcher.fetch
