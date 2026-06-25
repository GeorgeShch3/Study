package model

import "time"

type Employee struct {
	ID           uint       `gorm:"primaryKey;autoIncrement"`
	DepartmentID uint       `gorm:"not null;index"`
	FullName     string     `gorm:"not null;size:200"`
	Position     string     `gorm:"not null;size:200"`
	HiredAt      *time.Time
	CreatedAt    time.Time `gorm:"not null;default:now()"`
}
