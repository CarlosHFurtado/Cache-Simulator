#include <iostream>
#include <fstream>
#include <vector>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <string>
#include <stdexcept>

using namespace std;

enum class Replacement { LRU, FIFO, RANDOM };

struct LRU_Node {
    int line_index;
    LRU_Node* prev;
    LRU_Node* next;
};

struct FIFO_Queue {
    vector<int> queue;
    vector<bool> in_queue;
    int front = 0;
    int rear = -1;
    int size = 0;
    int capacity;

    FIFO_Queue(int cap) : capacity(cap) {
        queue.resize(capacity);
        in_queue.resize(capacity, false);
    }

    int enqueue(int line_index) {
        if (in_queue[line_index]) return 0;
        rear = (rear + 1) % capacity;
        queue[rear] = line_index;
        in_queue[line_index] = true;
        size++;
        return 0;
    }

    int dequeue() {
        int victim = queue[front];
        front = (front + 1) % capacity;
        in_queue[victim] = false;
        size--;
        return victim;
    }
};


struct Line {
    bool valid = false;
    uint32_t tag = 0;
    LRU_Node* lru_node = nullptr;
};

struct Set {
    vector<Line> lines;
    LRU_Node* lru_head = nullptr;
    LRU_Node* lru_tail = nullptr;
    FIFO_Queue* fifo = nullptr;

    Set(int assoc, Replacement repl) {
        lines.resize(assoc);
        if (repl == Replacement::FIFO) {
            fifo = new FIFO_Queue(assoc);
        }
    }

    ~Set() {
        if (fifo) delete fifo;
        while (lru_head) {
            LRU_Node* tmp = lru_head;
            lru_head = lru_head->next;
            delete tmp;
        }
    }

    void append_lru(LRU_Node* node) {
        if (!lru_head) {
            lru_head = lru_tail = node;
        } else {
            lru_tail->next = node;
            node->prev = lru_tail;
            lru_tail = node;
        }
    }

    void remove_lru(LRU_Node* node) {
        if (node->prev)
            node->prev->next = node->next;
        else
            lru_head = node->next;

        if (node->next)
            node->next->prev = node->prev;
        else
            lru_tail = node->prev;

        node->prev = node->next = nullptr;
    }
};

class Cache {
public:
    vector<Set*> sets;
    uint32_t n_sets, block_size, assoc;
    Replacement repl;
    uint32_t total_valid_lines = 0;
    uint32_t accesses = 0;
    uint32_t hits = 0;
    uint32_t misses = 0;
    uint32_t miss_compulsory = 0;
    uint32_t miss_capacity = 0;
    uint32_t miss_conflict = 0;

    Cache(uint32_t ns, uint32_t bs, uint32_t a, Replacement r)
        : n_sets(ns), block_size(bs), assoc(a), repl(r) {
        for (uint32_t i = 0; i < ns; ++i)
            sets.push_back(new Set(a, r));
    }

    ~Cache() {
        for (auto s : sets) delete s;
    }

    void access(uint32_t address) {
        uint32_t offset_bits = log2(block_size);
        uint32_t index_bits = log2(n_sets);
        uint32_t index = (address >> offset_bits) & ((1 << index_bits) - 1);
        uint32_t tag = address >> (offset_bits + index_bits);

        accesses++;
        Set* set = sets[index];

        // HIT
        for (uint32_t i = 0; i < assoc; ++i) {
            if (set->lines[i].valid && set->lines[i].tag == tag) {
                if (repl == Replacement::LRU && set->lines[i].lru_node) {
                    set->remove_lru(set->lines[i].lru_node);
                    set->append_lru(set->lines[i].lru_node);
                }
                hits++;
                return;
            }
        }
        misses++;

        // Compulsório
        for (uint32_t i = 0; i < assoc; ++i) {
            if (!set->lines[i].valid) {
                set->lines[i].valid = true;
                set->lines[i].tag = tag;

                if (repl == Replacement::LRU) {
                    auto* node = new LRU_Node{static_cast<int>(i), nullptr, nullptr};
                    set->lines[i].lru_node = node;
                    set->append_lru(node);
                } else if (repl == Replacement::FIFO) {
                    set->fifo->enqueue(i);
                }

                miss_compulsory++;
                total_valid_lines++;
                return;
            }
        }

        // Substituição
        int victim_index = -1;
        switch (repl) {
            case Replacement::RANDOM:
                victim_index = rand() % assoc;
                break;
            case Replacement::LRU:
                if (set->lru_head)
                    victim_index = set->lru_head->line_index;
                break;
            case Replacement::FIFO:
                victim_index = set->fifo->dequeue();
                break;
        }

        set->lines[victim_index].tag = tag;
        if (repl == Replacement::LRU && set->lines[victim_index].lru_node) {
            set->remove_lru(set->lines[victim_index].lru_node);
            set->append_lru(set->lines[victim_index].lru_node);
        } else if (repl == Replacement::FIFO) {
            set->fifo->enqueue(victim_index);
        }

        if (total_valid_lines == n_sets * assoc) {
            miss_capacity++;
        } else {
            miss_conflict++;
        }
    }

    void print_stats(bool modo) const {
        auto r = [](uint32_t n, uint32_t base) -> double {
            return (base == 0) ? 0.0 : static_cast<double>(n) / base;
        };

        if (modo) {
        // Modo compacto (flag = 1)
        printf("%u %.4lf %.4lf %.4lf %.4lf %.4lf\n",
               accesses, r(hits, accesses), r(misses, accesses),
               r(miss_compulsory, misses),
               r(miss_capacity, misses),
               r(miss_conflict, misses));
        } else {
        // Modo formatado (flag = 0)
        printf("==================================================================\n");
        printf("Total de acessos:            %u\n", accesses);
        printf("Taxa de hits:                %.2lf%%\n", 100.0 * r(hits, accesses));
        printf("Taxa de misses:              %.2lf%%\n", 100.0 * r(misses, accesses));
        printf("- Misses compulsórios:       %.2lf%%\n", 100.0 * r(miss_compulsory, misses));
        printf("- Misses por capacidade:     %.2lf%%\n", 100.0 * r(miss_capacity, misses));
        printf("- Misses por conflito:       %.2lf%%\n", 100.0 * r(miss_conflict, misses));
        printf("==================================================================\n");
        }
    }
};

uint32_t swap_endian(uint32_t value) {
    return ((value >> 24) & 0xFF) |
           ((value >> 8)  & 0xFF00) |
           ((value << 8)  & 0xFF0000) |
           ((value << 24) & 0xFF000000);
}

Replacement parse_replacement(const string& r) {
    if (r == "L") return Replacement::LRU;
    if (r == "F") return Replacement::FIFO;
    if (r == "R") return Replacement::RANDOM;
    throw invalid_argument("Invalid replacement policy: " + r);
}

void usage(const string& prog) {
    cout << "\nUsage: " << prog << " [nsets] [bsize] [assoc] [R|L|F] [0|1] [input_file]\n" << endl;
}

int main(int argc, char** argv) {
    srand(0);

    if (argc != 7) {
        usage(argv[0]);
        return 1;
    }

    uint32_t nsets = stoul(argv[1]);
    uint32_t bsize = stoul(argv[2]);
    uint32_t assoc = stoul(argv[3]);
    Replacement repl;
    try {
        repl = parse_replacement(argv[4]);
    } catch (const invalid_argument& e) {
        cerr << e.what() << endl;
        return 1;
    }

    bool flag = strcmp(argv[5], "0");
    string filename = argv[6];

    if ((uint64_t)nsets * bsize * assoc > UINT32_MAX) {
        cerr << "Erro: cache maior que espaço de endereçamento 32-bit" << endl;
        return 1;
    }

    ifstream infile(filename, ios::binary);
    if (!infile) {
        cerr << "Erro ao abrir arquivo: " << filename << endl;
        return 1;
    }

    Cache cache(nsets, bsize, assoc, repl);

    uint32_t address;
    while (infile.read(reinterpret_cast<char*>(&address), sizeof(uint32_t))) {
        cache.access(swap_endian(address));
    }

    cache.print_stats(flag);
    return 0;
}
