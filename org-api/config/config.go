package config

import (
	"fmt"
	"os"
	"strconv"
)

type Config struct {
	DBHost       string
	DBPort       string
	DBUser       string
	DBPassword   string
	DBName       string
	ServerPort   string
	MaxOpenConns int
	MaxIdleConns int
}

func Load() *Config {
	return &Config{
		DBHost:       getEnv("DB_HOST", "localhost"),
		DBPort:       getEnv("DB_PORT", "5432"),
		DBUser:       getEnv("DB_USER", "postgres"),
		DBPassword:   getEnv("DB_PASSWORD", "postgres"),
		DBName:       getEnv("DB_NAME", "orgapi"),
		ServerPort:   getEnv("SERVER_PORT", "8080"),
		MaxOpenConns: getEnvInt("DB_MAX_OPEN_CONNS", 25),
		MaxIdleConns: getEnvInt("DB_MAX_IDLE_CONNS", 5),
	}
}

func (c *Config) DSN() string {
	return fmt.Sprintf(
		"host=%s port=%s user=%s password=%s dbname=%s sslmode=disable",
		c.DBHost, c.DBPort, c.DBUser, c.DBPassword, c.DBName,
	)
}

func getEnv(key, def string) string {
	if v := os.Getenv(key); v != "" {
		return v
	}
	return def
}

func getEnvInt(key string, def int) int {
	if v := os.Getenv(key); v != "" {
		if n, err := strconv.Atoi(v); err == nil {
			return n
		}
	}
	return def
}
