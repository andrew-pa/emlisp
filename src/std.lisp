
(define (caar x) (car (car x)))
(define (cadr x) (car (cdr x)))
(define (cdar x) (cdr (car x)))
(define (cddr x) (cdr (cdr x)))

(define (eqv? a b) (eq? a b))

(define (length x)
  (if (nil? x) 0 (+ 1 (length (cdr x)))))

(define (equal? a b)
    (if (cons? a)
      (if (cons? b)
        (if (equal? (car a) (car b))
          (equal? (cdr a) (cdr b))
          #f)
        #f)
      (eq? a b)))

(define (memv el list)
    (if (nil? list)
        #f
        (if (eqv? el (car list))
          #t
          (memv el (cdr list)))))

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

(defmacro (case ... args)
  (let ([key (car args)]
        [branches (cdr args)])
    (if (nil? branches)
      '(macro-expand-error "case missing else")
      (let ([atoms (caar branches)]
            [result (car (cdar branches))]
            [rest (cdr branches)])
        (if (eq? atoms 'else)
          result
          `(if (memv ,key (quote ,atoms)) ,result (case ,key ,@rest)))))))

(define (append list el)
  (if (nil? list)
    (cons el #n)
    (cons (car list) (append (cdr list) el))))

(define (map proc list)
  (if (nil? list)
    #n
    (cons (proc (car list)) (map proc (cdr list)))))

(define (fold proc init list)
  (if (nil? list)
    init
    (proc (car list) (fold proc init (cdr list)))))
