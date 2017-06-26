
;; put at the beginning of a line. searches for [^:]", then puts three
;; spaces after the first quote on that line.
(fset 'indent-nonlabels
   [escape ?\C-S ?[ ?^ ?: ?] ?\" ?, ?\C-a tab right ?  ?  ?  ?\C-e right])


(fset 'error-handler
   [escape ?d escape ?d right ?  ?^ ?\S-  ?T ?A ?L ?U ?t ?i ?l ?. ?e ?r ?r ?o ?r ?  escape ?x ?i ?n ?s ?e ?r ?t ?- ?p ?o ?s ?i ?t ?i ?o ?n return ?  ?\" ?\C-y ?\" C-left backspace backspace ?\C-e])
