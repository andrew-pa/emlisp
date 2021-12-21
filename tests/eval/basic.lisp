(assert! #t "test assertion")

; test equality
(assert! (eq? #t #t) "test #t = #t")
(assert! (eq? #f #f) "test #f = #f")
(assert! (eq? 1234543 1234543) "test 1234543 = 1234543")
(assert! (eq? 'a 'a) "test 'a = 'a")
; strings currently are not deduped, so the two strings will get different addresses and so are not eq?
(assert! (eq? #f (eq? "asdf" "asdf")) "test \"asdf\" != \"asdf\"")

(set! x "asdf")
(assert! (eq? x x) "test same string eq?")
(set! x (cons 'a 'b))
(assert! (eq? x x) "test same cons eq?")

; test quote/car/cdr
(set! x '(a b c))
(assert! (eq? (car x) 'a))
(assert! (eq? (car (cdr x)) 'b))
(assert! (eq? (car (cdr (cdr x))) 'c))
(assert! (eq? (cdr (cdr (cdr x))) #n))

