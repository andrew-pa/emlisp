(define (equal? a b)
    (if (cons? a)
      (if (cons? b)
        (if (equal? (car a) (car b))
          (equal? (cdr a) (cdr b))
          #f)
        #f)
      (eq? a b)))

(defmacro (trivial) '(a b))
(assert! (equal? '(trivial) '(a b)))

(defmacro (twice x) `(cons ,x ,x))
(assert! (equal? '(twice w) '(cons w w)))
(assert! (equal? (twice 3) (cons 3 3)))

(defmacro (or ... values)
  (if (nil? values) #f
    (if (nil? (cdr values)) (car values)
      (let ([X (unique-symbol "x")])
        `(let ([,X ,(car values)]) (if ,X ,X (or ,@(cdr values))))))))

(defmacro (and ... values) (if (nil? values) #t (if (nil? (cdr values)) (car values) `(if ,(car values) (and ,@(cdr values)) #f) )))

(assert-eq! (and #t #t #t #t) #t)
(assert-eq! (and #t #t #f #t) #f)

(assert-eq! (or #f #t) #t)
(assert-eq! (or #f #f #f #t) #t)
