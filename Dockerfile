FROM gcc:latest AS cpp-build

WORKDIR /app

COPY . .

RUN apt-get update && apt-get install -y \
    g++ \
    libboost-system-dev \
    libboost-thread-dev \
    libsqlite3-dev

RUN g++ -o main main.cpp -lboost_system -lpthread -lsqlite3


RUN pip install --no-cache-dir -r requirements.txt

EXPOSE 12345

CMD ["./main"]

