
(define test 3)

(defmacro (and ... values)
  (if (nil? values)
    #t
    (if (nil? (cdr values))
      (car values)
      `(if ,(car values)
         (and ,@(cdr values))
         #f)
    )))

(define (equal? a b)
    (if (cons? a)
      (if (cons? b)
        (if (equal? (car a) (car b))
          (equal? (cdr a) (cdr b))
          #f)
        #f)
      (eq? a b)))


