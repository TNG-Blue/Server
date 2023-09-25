FROM gcc:latest

WORKDIR /app

COPY . .

RUN apt-get update && apt-get install -y libboost-system-dev libboost-thread-dev libsqlite3-dev

RUN g++ -o main main.cpp -lboost_system -lpthread -lsqlite3

CMD ["./main"]
