package main

import (
	"crypto/md5"
	"encoding/hex"
	"encoding/json"
	"fmt"
	"log"
	"time"

	"github.com/gin-gonic/gin"
	"github.com/go-redis/redis"
)

type RAM struct {
	Address int
	Value   int
}

type Instruction struct {
	Name     string
	Argument int
}

type Program struct {
	Id           int
	Name         string
	Instructions []Instruction
	Memory       []RAM
	ClockSpeed   int
	Time         int
}

var redisClient *redis.Client

func main() {
	var id int

	redisClient = redis.NewClient(&redis.Options{
		Addr:     "localhost:6379",
		Password: "",
		DB:       0,
	})

	pong, err := redisClient.Ping().Result()
	fmt.Println(pong, err)

	router := gin.Default()

	router.POST("/program", func(c *gin.Context) {
		var program Program
		if err := c.BindJSON(&program); err != nil {
			c.AbortWithError(500, err)
		}

		program.Id = id
		id++

		log.Printf("%v", program)
		bytes, _ := json.Marshal(program)
		sum := md5.Sum(bytes)
		hash := hex.EncodeToString(sum[:])

		redisClient.Set(hash, bytes, time.Hour*48)
		z := redis.Z{
			Score:  900,
			Member: hash,
		}
		redisClient.ZAdd("queue", z)
		c.String(200, hash)
	})

	router.GET("/program/:id", func(c *gin.Context) {
		c.IndentedJSON(200, getProgram(c.Param("id")))
	})

	router.GET("/programs", func(c *gin.Context) {
		cmd := redisClient.ZRangeWithScores("queue", 0, 1000)
		values, _ := cmd.Result()
		c.IndentedJSON(200, values)
	})

	router.GET("/run", func(c *gin.Context) {
		cmd := redisClient.ZPopMin("queue", 1)
		item, _ := cmd.Result()
		if len(item) > 0 {
			program := getProgram(item[0].Member.(string))
			c.IndentedJSON(200, program)
		} else {
			c.Status(404)
		}
	})

	router.Run("0.0.0.0:8102")
}

func getProgram(hash string) Program {
	cmd := redisClient.Get(hash)
	value, _ := cmd.Result()
	program := Program{}
	json.Unmarshal([]byte(value), &program)
	return program
}
