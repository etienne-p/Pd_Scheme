(define (bang) (outlet "bang_received"))

(define (my-function a b c) (outlet a (+ b 1) (* c 4.2)))
