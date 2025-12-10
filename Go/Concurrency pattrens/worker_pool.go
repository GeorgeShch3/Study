package main

import (
	"fmt"
	"time"
)

func worker(id int, jobs <- chan int, res chan <- int) {
	for i := range jobs {
		fmt.Println("worker ", id, "Start job", i)
		time.Sleep(time.Second * time.Duration(2))
		fmt.Println("worker ", id, "Finish job", i)
		res <- i * 2
	}
}

func main() {
	jobs := make(chan int)
	res := make(chan int)
	defer close(jobs)
	
	for i := 1; i < 4; i++ {
		go worker(i, jobs, res)
	}

	go func() {
		for i := 1; i < 10; i++ {
			jobs <- i
		}
	}()

	for i := 1; i < 10; i++ {
		fmt.Println("result ", <-res)
	}
}