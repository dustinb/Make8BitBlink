FROM golang

WORKDIR /app
COPY . .
RUN go mod vendor
RUN go build -o api
EXPOSE 8102

CMD ["./api"]