package main 

import (
	"fmt"
)

func Add(inputCh chan int) chan int {
	res := make(chan int)
	
	go func() {
		defer close(res)

		for val := range inputCh {
			res <- val 
		}
	}()
	return res
}

func Mul(inputCh chan int) chan int {
	res := make(chan int)

	go func() {
		defer close(res)

		for val := range inputCh {
			res <- val * 2
		}
	}()
	return res
}

func generate(nums []int) chan int {
	res := make(chan int)
	
	go func() {
		defer close(res)
		for _, n := range nums {
			res <- n + 2
		}
	}()
	return res
}

func main() {
	values := []int{1,2,3,4,5}
	for n := range Mul(Add(generate(values))) {
		fmt.Println(n)
	}
}