
(set! id (lambda (x) x))
(assert-eq! 'a (id 'a) "identity function")

(set! revcons (lambda (x y) (cons y x)))
(set! x (revcons 'a 'b))
(assert-eq! 'a (car x) "function with two arguments 1")
(assert-eq! 'b (car (cdr x)) "function with two arguments 2")

(set! test-closure-f (lambda (x) (lambda (y) (cons x y))))
(set! test-closure (test-closure-f 'hello))
(assert-eq! (test-closure) 'hello)
(assert-eq! ((test-closure-f 2)) 2)
(assert-eq! (test-closure) 'hello)

(set! equal?
  (lambda (a b)
    (if (cons? a)
      (if (cons? b)
        (if (equal? (car a) (car b))
          (equal? (cdr a) (cdr b))
          #f)
        #f)
      (eq? a b)))

(set! mut-closure-f
  (lambda (x)
    (lambda (msg) 
      (if (eq? msg 'get) x
        (if (eq? msg 'inc) (set! x (cons x 2))
          #n))))

(set! test-closure (mut-closure-f 3))
(assert-eq! (test-closure 'get) 3)
(assert-eq! (test-closure 'inc) #n)
(assert-eq! (test-closure 'get) 3)
