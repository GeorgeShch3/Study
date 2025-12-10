package main

import (
	"fmt"
	"sync"
	"time"
)

type Semaphore struct {
	sem chan struct{}
}

func NewSemaphore(SizeBuff int) Semaphore {
	return Semaphore{
		sem : make(chan struct{}, SizeBuff),
	}
}

func (s *Semaphore) Up() {
	s.sem <- struct{}{}
}

func (s *Semaphore) Down() {
	<- s.sem
}

func main() {
	s := NewSemaphore(5)
	wg := sync.WaitGroup{}

	for i := 0; i < 10; i++ {
		s.Up()
		wg.Add(1)
		go func(i int) {
			defer func(){
				s.Down()
				wg.Done()
			}()
			time.Sleep(time.Duration(i) * time.Second)
			fmt.Println(i)
		}(i)
	}
	wg.Wait()
}