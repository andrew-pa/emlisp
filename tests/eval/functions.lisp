
(set! id (lambda (x) x))
(assert-eq! 'a (id 'a) "identity function")

(set! revcons (lambda (x y) (cons y x)))
(set! x (revcons 'a 'b))
(assert-eq! 'b (car x) "function with two arguments 1")
(assert-eq! 'a (cdr x) "function with two arguments 2")

(define (equal? a b)
    (if (cons? a)
      (if (cons? b)
        (if (equal? (car a) (car b))
          (equal? (cdr a) (cdr b))
          #f)
        #f)
      (eq? a b)))

; make sure equal? actually works
(assert! (equal? '(a b c) '(a b c)))
(assert! (equal? '(a (b (c))) '(a (b (c)))))

(set! test-closure-f (lambda (x) (lambda (y) (cons x y))))
(set! test-closure (test-closure-f 'hello))
(assert! (equal? (test-closure 3) (cons 'hello 3)))
(assert! (equal? (test-closure 5) (cons 'hello 5)))
(assert! (equal? ((test-closure-f 5) 7) (cons 5 7)))

(define (mut-closure-f x)
    (lambda (msg) 
      (if (eq? msg 'get) x
        (if (eq? msg 'inc) (set! x (cons x 2))
          #n))))

(set! test-closure (mut-closure-f 3))
(assert-eq! (test-closure 'get) 3)
(assert-eq! (test-closure 'inc) #n)
(assert! (equal? (test-closure 'get) (cons 3 2)))
(assert-eq! (test-closure 'inc) #n)
(assert! (equal? (test-closure 'get) (cons (cons 3 2) 2)))
