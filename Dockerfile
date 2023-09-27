FROM gcc:latest AS cpp-build

WORKDIR /app

COPY . .

RUN apt-get update && apt-get install -y libboost-system-dev libboost-thread-dev libsqlite3-dev

RUN g++ -o main main.cpp -lboost_system -lpthread -lsqlite3



WORKDIR /app

COPY --from=cpp-build /app/main /app/main

COPY requirements.txt .
COPY . .

RUN pip install --no-cache-dir -r requirements.txt

CMD ["./main"]
