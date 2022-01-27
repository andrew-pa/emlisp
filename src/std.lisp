
(define (caar x) (car (car x)))
(define (cadr x) (car (cdr x)))
(define (cdar x) (cdr (car x)))
(define (cddr x) (cdr (cdr x)))

(defmacro (and ... values)
  (if (nil? values)
    #t
    (if (nil? (cdr values))
      (car values)
      `(if ,(car values)
         (and ,@(cdr values))
         #f)
    )))

(defmacro (or ... values)
  (if (nil? values)
    #f
    (if (nil? (cdr values))
      (car values)
      (let ([X (unique-symbol x)])
        `(let ([,X ,(car values)]) (if ,X ,X (or ,@(cdr values)))))
    )))

(defmacro (cond ... branches)
  (if (nil? branches) '(macro-expand-error "cond missing else")
  (let ([test (caar branches)]
        [result (car (cdar branches))]
        [rest (cdr branches)])
    (if (eq? test 'else)
        result
        `(if ,test ,result (cond ,@rest))))))


(define (equal? a b)
    (if (cons? a)
      (if (cons? b)
        (if (equal? (car a) (car b))
          (equal? (cdr a) (cdr b))
          #f)
        #f)
      (eq? a b)))


