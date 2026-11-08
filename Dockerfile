FROM gcc:13

WORKDIR /app

COPY main.cpp .
COPY httplib.h .

RUN g++ main.cpp -o rideeaze -std=c++17 -pthread

EXPOSE 8080

CMD ["sh", "-c", "./rideeaze"]