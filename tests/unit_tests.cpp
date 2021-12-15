#define CATCH_CONFIG_MAIN
#include <string>
#include <thread>
#include <chrono>
#include <vector>
#include <atomic>

#include "catch.hpp"
#include "code_machina/BlockingCollection.h"
using namespace code_machina;

typedef struct Data {
    static int constructed;
    Data() { constructed++; };
    Data(std::string a) : t(a) { constructed++; };
    Data(const Data &other) noexcept : t(other.t) { constructed++; };
    Data(Data &&other) noexcept : t(std::move(other.t)) { constructed++; };
    Data &operator=(const Data &other) noexcept {
        t = other.t;
        return *this;
    };
    Data &operator=(Data &&other) noexcept {
        t = std::move(other.t);
        return *this;
    };
    bool        operator==(const Data &other) { return t == other.t; }
    std::string t{ "test" };
    ~Data() { constructed--; }
} Data;
int Data::constructed = 0;

TEST_CASE("simple test") {
    CHECK(Data::constructed == 0);
    {
        BlockingCollection<Data> q(100);
        for (int i = 0; i < 10; i++) { q.emplace("test"); }
        CHECK((q.size() == 10 && !q.is_empty()));
        CHECK(Data::constructed == 10);

        Data t;
        q.take(t);
        CHECK((q.size() == 9 && !q.is_empty()));
        CHECK(Data::constructed == 10);

        q.take(t);
        q.emplace("test");
        CHECK((q.size() == 9 && !q.is_empty()));
        CHECK(Data::constructed == 10);
        CHECK(q.history_size() == 11);
    }
    CHECK(Data::constructed == 0);
}

TEST_CASE("try add take") {
    BlockingCollection<int> q(1);
    int                     t = 0;
    CHECK((q.try_add(1) == BlockingCollectionStatus::Ok));
    CHECK((q.size() == 1 && !q.is_empty()));
    CHECK((q.try_add(2) == BlockingCollectionStatus::TimedOut));
    CHECK((q.size() == 1 && !q.is_empty()));
    CHECK((q.try_take(t) == BlockingCollectionStatus::Ok && t == 1));
    CHECK((q.size() == 0 && q.is_empty()));
    CHECK((q.try_take(t) == BlockingCollectionStatus::TimedOut && t == 1));
    CHECK((q.size() == 0 && q.is_empty()));
}

TEST_CASE("copyable") {
    // Copyable only type

    struct Test {
        Test() {}
        Test(const Test &) noexcept {}
        Test &operator=(const Test &) noexcept { return *this; }
        Test(Test &&) = delete;
    };
    BlockingCollection<Test> q(16);
    // lvalue
    Test v;
    q.emplace(v);
    q.try_emplace(v);
    q.add(v);
    q.try_add(v);

    // compile error
    // q.add(Test());
    // q.try_add(Test());
}

TEST_CASE("moveable") {
    BlockingCollection<std::unique_ptr<int>> q(16);
    // lvalue
    auto v = std::unique_ptr<int>(new int(1));

    // compile error
    // q.emplace(v);
    // q.try_emplace(v);
    // q.add(v);
    // q.try_add(v);
    q.emplace(std::move(v));

    // xvalue
    q.emplace(std::unique_ptr<int>(new int(1)));
    q.try_emplace(std::unique_ptr<int>(new int(1)));
    q.add(std::unique_ptr<int>(new int(1)));
    q.try_add(std::unique_ptr<int>(new int(1)));
}

TEST_CASE("2 thread") {
    BlockingCollection<Data> collection(11);
    // a simple blocking consumer
    std::thread consumer_thread([&collection]() {
        Data t;
        collection.at(t, 6);
        CHECK((t == Data{ "6" }));
        collection.at(t, 7);
        CHECK((t == Data{ "7" }));
        collection.at(t, 8);
        CHECK((t == Data{ "8" }));
        collection.at(t, 9);
        CHECK((t == Data{ "9" }));
        collection.at(t, 10);
        CHECK((t == Data{ "10" }));

        auto status = collection.at(t, 11);
        CHECK((status == BlockingCollectionStatus::AtExceedCapacity));

        int i = 0;
        while (!collection.is_completed()) {
            Data data;

            // take will block if there is no data to be taken
            auto status = collection.take(data);

            if (status == BlockingCollectionStatus::Ok) {
                CHECK(data.t == std::to_string(i));
                i++;
            }

            // Status can also return BlockingCollectionStatus::Completed meaning take was called
            // on a completed collection. Some other thread can call complete_adding after we pass
            // the is_completed check but before we call take. In this example, we can ignore that
            // status since the loop will break on the next iteration.
        }
        CHECK(i == 11);
        CHECK(collection.history_size() == 11);
    });

    // a simple blocking producer
    std::thread producer_thread([&collection]() {
        // blocks if collection.size() == collection.bounded_capacity()
        collection.emplace("0");

        std::vector<Data> data_v1{ { "1" }, { "2" }, { "3" } };
        size_t            added;
        collection.add_bulk(data_v1.begin(), data_v1.end(), added);

        // return immediately if collection is full
        collection.try_add({ "4" });

        // add with timeout
        collection.try_add(Data{ "5" }, std::chrono::milliseconds(1000));

        // emplace several rvalue
        collection.try_emplace("6");

        // emplace several rvalue with timeout
        collection.try_emplace_timed(std::chrono::milliseconds(1000), "7");

        std::vector<Data> data_v2{ { "8" }, { "9" }, { "10" } };

        collection.try_add_bulk(data_v2.begin(), data_v2.end(), added);

        // let consumer know we are done
        collection.complete_adding();
    });
    producer_thread.join();
    consumer_thread.join();
}

TEST_CASE("at") {
    // A bounded collection. It can hold no more
    // than 100 items at once.
    BlockingCollection<int> collection(10);

    // a simple blocking consumer
    std::thread consumer_thread([&collection]() {
        int data;
        // fetch the item of index 0, if no data return immediately
        auto status = collection.try_at(data, 0);
        CHECK(status == BlockingCollectionStatus::TimedOut);

        // fetch will block if there is no data at index 1
        status = collection.at(data, 1);
        CHECK(status == BlockingCollectionStatus::Ok);
        CHECK(data == 1);

        // fetch the item of index with timeout
        status = collection.try_at(data, 2, std::chrono::milliseconds(10));
        CHECK(status == BlockingCollectionStatus::TimedOut);

        status = collection.try_at(data, 3, std::chrono::milliseconds(10));
        CHECK(status == BlockingCollectionStatus::Ok);
        CHECK(data == 3);
    });

    // a simple blocking producer
    std::thread producer_thread([&collection]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        // blocks if collection.size() == collection.bounded_capacity()
        collection.add(0);
        collection.add(1);
        std::this_thread::sleep_for(std::chrono::milliseconds(15));
        collection.add(2);
        collection.add(3);
    });

    producer_thread.join();
    consumer_thread.join();
}

TEST_CASE("fuzz") {
    const uint64_t               numOps     = 1000;
    const uint64_t               numThreads = 10;
    BlockingCollection<uint64_t> q(numThreads);
    std::atomic<bool>            flag(false);
    std::vector<std::thread>     threads;
    std::atomic<uint64_t>        sum(0);
    for (uint64_t i = 0; i < numThreads; ++i) {
        threads.push_back(std::thread([&, i] {
            while (!flag)
                ;
            for (auto j = i; j < numOps; j += numThreads) { q.add(j); }
        }));
    }
    for (uint64_t i = 0; i < numThreads; ++i) {
        threads.push_back(std::thread([&, i] {
            while (!flag)
                ;
            uint64_t threadSum = 0;
            for (auto j = i; j < numOps; j += numThreads) {
                uint64_t v;
                q.take(v);
                threadSum += v;
            }
            sum += threadSum;
        }));
    }
    flag = true;
    for (auto &thread : threads) { thread.join(); }
    assert(sum == numOps * (numOps - 1) / 2);
}
