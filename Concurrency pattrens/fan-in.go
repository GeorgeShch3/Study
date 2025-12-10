package main 

import (
	"fmt"
	"sync"
)

func MergeChannels[T any](channels ...<-chan T) <-chan T {
	res := make(chan T)
	wg := sync.WaitGroup{}

	for _, ch := range channels {
		wg.Add(1)
		go func(ch <-chan T) {
			defer wg.Done()
			for c := range ch {
				res <- c
			}
		}(ch)
	}
	go func() {
		wg.Wait()
		close(res)
	}()
	return res
}

func main() {
	ch1 := make(chan int)
	ch2 := make(chan int)
	ch3 := make(chan int)

	go func() {
		defer func() {
			close(ch1)
			close(ch2)
			close(ch3)
		}()

		for i := 0; i < 10; i++ {
			ch1 <- 1
			ch2 <- 2
			ch3 <- 3
		}
	}()

	res := MergeChannels(ch1, ch2, ch3)

	for c := range res {
		fmt.Println(c)
	}
}