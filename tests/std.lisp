
(assert! (equal? '(a ((b) (c))) '(a ((b) (c)))))

(assert! (memv 'a '(c a b)))

(assert! (and))
(assert! (and #t))
(assert! (and #t #t))
(assert! (and #t #t #t))
(assert-eq! (and #t #t #f) #f)

(assert-eq! (or) #f)
(assert! (or #t))
(assert! (or #f #t))
(assert! (or #f #t #f))

(assert-eq!
  (cond
    [else 3])
  3)

(assert-eq!
  (cond
    [#f 6]
    [else 3])
  3)

(assert-eq!
  (cond
    [#f 9]
    [#t 8]
    [else 3])
  8)

(define (test-case sym)
  (case sym
    [(x) 3]
    [(y) 7]
    [(z) 8]
    [else 100]))

(assert-eq! (test-case 'x) 3)
(assert-eq! (test-case 'y) 7)
(assert-eq! (test-case 'z) 8)
(assert-eq! (test-case 'a) 100)

(assert! (equal? (append '(1 2 3) 4) '(1 2 3 4)))

(assert! (equal? (map (lambda (x) x) '(1 2)) '(1 2)))

(assert! (equal? (map (lambda (x) 'z) '(1 2)) '(z z)))

(assert! (equal? (fold (lambda (a b) (cons a b)) #n '(1 2 3)) '(1 2 3)))
