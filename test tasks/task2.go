package main

import (
	"bufio"
	"container/heap"
	"encoding/csv"
	"fmt"
	"io"
	"math"
	"os"
	"sort"
	"strconv"
	"strings"
)

const (
	FloatToInt    = 1000000.0
	defaultK      = 5
	EarthRadiusKm = 6371.0
)

type Point struct {
	Lat int32
	Lon int32
}

type Node struct {
	P       Point
	Radius  float64
	Closer  *Node
	Further *Node
}

type VPTree struct {
	Root *Node
	Size int
}

func New(points []Point) *VPTree {
	if len(points) == 0 {
		return &VPTree{Root: nil, Size: 0}
	}

	root := buildVPTree(points)
	return &VPTree{
		Root: root,
		Size: len(points),
	}
}

func haversineDistance(p1, p2 Point) float64 {
	tmp := math.Pi / (180.0 * FloatToInt)
	lat1 := float64(p1.Lat) * tmp
	lon1 := float64(p1.Lon) * tmp
	lat2 := float64(p2.Lat) * tmp
	lon2 := float64(p2.Lon) * tmp

	dlat := lat2 - lat1
	dlon := lon2 - lon1

	a := math.Pow(math.Sin(dlat/2), 2) + math.Cos(lat1)*math.Cos(lat2)*math.Pow(math.Sin(dlon/2), 2)
	c := 2 * math.Atan2(math.Sqrt(a), math.Sqrt(1-a))

	return c
}

func buildVPTree(points []Point) *Node {
	if len(points) == 0 {
		return nil
	}

	node := &Node{
		P: points[0],
	}

	if len(points) == 1 {
		return node
	}

	type pointDist struct {
		point Point
		dist  float64
	}

	pointDists := make([]pointDist, len(points)-1)
	for i := 1; i < len(points); i++ {
		pointDists[i-1] = pointDist{
			point: points[i],
			dist:  haversineDistance(node.P, points[i]),
		}
	}

	sort.Slice(pointDists, func(i, j int) bool {
		return pointDists[i].dist < pointDists[j].dist
	})

	medianIndex := len(pointDists) / 2
	node.Radius = pointDists[medianIndex].dist

	closer := make([]Point, medianIndex+1)
	further := make([]Point, len(pointDists)-medianIndex-1)

	for i := 0; i <= medianIndex; i++ {
		closer[i] = pointDists[i].point
	}

	for i := medianIndex + 1; i < len(pointDists); i++ {
		further[i-medianIndex-1] = pointDists[i].point
	}

	node.Closer = buildVPTree(closer)
	node.Further = buildVPTree(further)

	return node
}

type SearchResult struct {
	Point       Point
	DistanceRad float64
	DistanceKm  float64
}

type PriorityQueue []SearchResult

func (pq PriorityQueue) Len() int            { return len(pq) }
func (pq PriorityQueue) Less(i, j int) bool  { return pq[i].DistanceRad > pq[j].DistanceRad }
func (pq PriorityQueue) Swap(i, j int)       { pq[i], pq[j] = pq[j], pq[i] }
func (pq *PriorityQueue) Push(x interface{}) { *pq = append(*pq, x.(SearchResult)) }
func (pq *PriorityQueue) Pop() interface{} {
	old := *pq
	n := len(old)
	x := old[n-1]
	*pq = old[0 : n-1]
	return x
}

func (t *VPTree) FindKNN(target Point, k int) []SearchResult {
	if t.Root == nil || k <= 0 {
		return []SearchResult{}
	}

	pq := make(PriorityQueue, 0, k)
	heap.Init(&pq)

	t.searchKNN(t.Root, target, k, &pq)

	res := make([]SearchResult, 0, pq.Len())
	for pq.Len() > 0 {
		res = append([]SearchResult{heap.Pop(&pq).(SearchResult)}, res...)
	}

	return res
}

func (t *VPTree) searchKNN(node *Node, target Point, k int, pq *PriorityQueue) {
	if node == nil {
		return
	}

	distRad := haversineDistance(target, node.P)
	distKm := distRad * EarthRadiusKm

	if pq.Len() < k {
		heap.Push(pq, SearchResult{Point: node.P, DistanceRad: distRad, DistanceKm: distKm})
	} else if distRad < (*pq)[0].DistanceRad {
		heap.Pop(pq)
		heap.Push(pq, SearchResult{Point: node.P, DistanceRad: distRad, DistanceKm: distKm})
	}

	var first, second *Node
	if distRad < node.Radius {
		first = node.Closer
		second = node.Further
	} else {
		first = node.Further
		second = node.Closer
	}

	t.searchKNN(first, target, k, pq)

	if second != nil {
		var minPossibleDist float64
		if distRad < node.Radius {
			minPossibleDist = node.Radius - distRad
		} else {
			minPossibleDist = distRad - node.Radius
		}

		if pq.Len() < k || minPossibleDist < (*pq)[0].DistanceRad {
			t.searchKNN(second, target, k, pq)
		}
	}
}

func ReadFile(filename string) ([]Point, error) {
	file, err := os.Open(filename)
	defer file.Close()
	if err != nil {
		return nil, fmt.Errorf("Ошибка открытия файла: %w", err)
	}

	reader := csv.NewReader(file)
	var points []Point

	for {
		record, err := reader.Read()
		if err == io.EOF {
			break
		}
		if err != nil {
			return nil, fmt.Errorf("Ошибка чтения CSV: %w", err)
		}

		lat, err := strconv.ParseFloat(record[0], 64)
		if err != nil {
			fmt.Printf("Ошибка чтения широты: %v\n", err)
			continue
		}

		lon, err := strconv.ParseFloat(record[1], 64)
		if err != nil {
			fmt.Printf("Ошибка чтения долготы: %v\n", err)
			continue
		}

		points = append(points, Point{Lat: int32(lat * FloatToInt), Lon: int32(lon * FloatToInt)})
	}

	fmt.Printf("Загружено точек: %d\n", len(points))
	return points, nil
}

func printResult(nearestPoints []SearchResult, targetPoint Point, lat float64, lon float64) {
	fmt.Printf("\nРезультаты поиска для (%.6f, %.6f):\n", lat, lon)
	if len(nearestPoints) == 0 {
		fmt.Println("Точки не найдены")
	} else {
		for _, p := range nearestPoints {
			pLat := float64(p.Point.Lat) / FloatToInt
			pLon := float64(p.Point.Lon) / FloatToInt
			fmt.Printf("(%.6f, %.6f) расстояние: %.6f км\n", pLat, pLon, p.DistanceKm)
		}
	}
	fmt.Println()
}

func run(tree *VPTree) {
	scanner := bufio.NewScanner(os.Stdin)
	fmt.Println("Введите координаты в формате: широта долгота")
	fmt.Println("Пример: 55.848662 37.581912")
	fmt.Println("Для выхода введите 'q'")

	for {
		fmt.Print("> ")
		if !scanner.Scan() {
			break
		}

		input := strings.TrimSpace(scanner.Text())

		if input == "q" {
			fmt.Println("Выход")
			break
		}

		record := strings.Fields(input)
		if len(record) != 2 {
			fmt.Println("Ошибка: введите 2 координаты (широта и долгота)")
			continue
		}

		lat, err := strconv.ParseFloat(record[0], 64)
		if err != nil {
			fmt.Printf("Ошибка формата широты '%s': %v\n", record[0], err)
			continue
		}

		lon, err := strconv.ParseFloat(record[1], 64)
		if err != nil {
			fmt.Printf("Ошибка формата долготы '%s': %v\n", record[1], err)
			continue
		}

		if lat < -90 || lat > 90 {
			fmt.Println("Ошибка: широта должна быть в диапазоне [-90, 90]")
			continue
		}
		if lon < -180 || lon > 180 {
			fmt.Println("Ошибка: долгота должна быть в диапазоне [-180, 180]")
			continue
		}

		targetPoint := Point{
			Lat: int32(lat * FloatToInt),
			Lon: int32(lon * FloatToInt),
		}

		nearestPoints := tree.FindKNN(targetPoint, defaultK)
		printResult(nearestPoints, targetPoint, lat, lon)
	}
}

func main() {
	if len(os.Args) < 2 {
		fmt.Println("Для запуска необходим файл .csv, пример:\ngo run task1.go coordinates.csv")
		os.Exit(1)
	}

	points, err := ReadFile(os.Args[1])
	if err != nil {
		fmt.Printf("Ошибка при чтении файла: %v\n", err)
		os.Exit(1)
	}

	fmt.Println("Обработка точек...")
	tree := New(points)
	fmt.Println("Обработка точек завершена")

	run(tree)
}
