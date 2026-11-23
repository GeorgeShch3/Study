package model

import "time"

type Department struct {
	ID        uint      `gorm:"primaryKey;autoIncrement"`
	Name      string    `gorm:"not null;size:200"`
	ParentID  *uint     `gorm:"index"`
	CreatedAt time.Time `gorm:"not null;default:now()"`
}
