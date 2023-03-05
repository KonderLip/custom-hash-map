#pragma once

#include <cstdint>
#include <functional>
#include <list>
#include <stdexcept>
#include <vector>

/// Hash map is an associative container that contains key-value pairs with unique keys. Search, insertion, and removal
/// of elements have average constant-time complexity. A strategy of collision resolution is coalesced hashing with the
/// cellar.
template <class Key, class T, class Hash = std::hash<Key>>
class HashMap {
public:
    /// Public typedefs:

    using KeyType = Key;
    using MappedType = T;
    using ValueType = std::pair<const Key, T>;
    using SizeType = size_t;
    using Hasher = Hash;

    /// Since values are contained in the list, its iterators are used. Iterator-related typedefs:

    using iterator = typename std::list<ValueType>::iterator;              // NOLINT
    using const_iterator = typename std::list<ValueType>::const_iterator;  // NOLINT

    /// Default constructor creates no elements.
    explicit HashMap(const Hasher &hf = Hasher()) : hasher_(hf) {
        Rehash(0);
    }

    /// Create an hash map consisting of copies of the elements from [first, last).
    template <typename InputIterator>
    HashMap(InputIterator first, InputIterator last, const Hasher &hf = Hasher()) : HashMap(hf) {
        for (auto it = first; it != last; ++it) {
            values_.push_back(*it);
        }
        Rehash(values_.size() << 1ull);
    }

    /// Create an hash map consisting of copies of the elements in the list.
    HashMap(std::initializer_list<std::pair<KeyType, MappedType>> list, const Hasher &hf = Hasher()) : HashMap(hf) {
        auto list_iterator = list.begin();
        for (size_t i = 0; i < list.size(); ++i) {
            values_.push_back(*list_iterator);
            ++list_iterator;
        }
        Rehash(values_.size() << 1ull);
    }

    /// Copy constructor.
    HashMap(const HashMap &other) {
        values_.clear();
        positions_.clear();
        element_count_ = 0;
        primary_size_ = other.primary_size_;
        cellar_size_ = other.cellar_size_;
        start_pos_ = primary_size_ + cellar_size_ - 1;
        hasher_ = other.hasher_;
        std::list<ValueType> values(other.values_);
        data_.assign(primary_size_ + cellar_size_, Data());
        for (const auto &x : values) {
            Insert(x, hasher_(x.first) % primary_size_);
        }
    }

    /// Copy assignment operator.
    HashMap &operator=(const HashMap &other) {
        if (this->values_ != other.values_) {
            values_.clear();
            positions_.clear();
            element_count_ = 0;
            primary_size_ = other.primary_size_;
            cellar_size_ = other.cellar_size_;
            start_pos_ = primary_size_ + cellar_size_ - 1;
            hasher_ = other.hasher_;
            std::list<ValueType> values(other.values_);
            data_.assign(primary_size_ + cellar_size_, Data());
            for (const auto &x : values) {
                Insert(x, hasher_(x.first) % primary_size_);
            }
        }
        return *this;
    }

    /// Returns the number of elements.
    SizeType size() const {
        return element_count_;
    }

    /// Checks whether the container is empty.
    bool empty() const {
        return size() == 0;
    }

    /// Clears the contents.
    void clear() {
        values_.clear();
        for (auto pos : positions_) {
            data_[pos] = Data();
        }
        positions_.clear();
        element_count_ = 0;
        start_pos_ = primary_size_ + cellar_size_ - 1;
    }

    /// Inserts elements.
    void insert(const ValueType &x) {
        SizeType h = hasher_(x.first) % primary_size_;
        if (Find(x.first, h) == NONE) {
            Insert(x, h);
        }
    }

    /// Erases elements.
    void erase(const KeyType &key) {
        SizeType pos = Find(key, hasher_(key) % primary_size_);
        if (pos != NONE) {
            data_[positions_.back()].rev_pos = data_[pos].rev_pos;
            std::swap(positions_[data_[pos].rev_pos], positions_.back());
            positions_.pop_back();
            values_.erase(data_[pos].value);
            data_[pos].value = iterator();
            data_[pos].used = false;
            data_[pos].deleted = true;
            --element_count_;
        }
    }

    /// Access specified element with bounds checking.
    const MappedType &at(const KeyType &key) const {
        auto it = find(key);
        if (it == end()) {
            throw std::out_of_range("_Map_base::at");
        }
        return it->second;
    }

    /// Access or insert specified element.
    MappedType &operator[](const KeyType &key) {
        SizeType h = hasher_(key) % primary_size_;
        SizeType pos = Find(key, h);
        if (pos != NONE) {
            return data_[pos].value->second;
        }
        return data_[Insert({key, MappedType()}, h)].value->second;
    }

    /// Finds element with specific key.
    iterator find(const KeyType &key) {
        SizeType pos = Find(key, hasher_(key) % primary_size_);
        if (pos == NONE) {
            return end();
        }
        return data_[pos].value;
    }

    /// Finds element with specific key.
    const_iterator find(const KeyType &key) const {
        SizeType pos = Find(key, hasher_(key) % primary_size_);
        if (pos == NONE) {
            return end();
        }
        return data_[pos].value;
    }

    /// Returns a read/write iterator that points to the first element in the hash map.
    iterator begin() {
        return values_.begin();
    }

    /// Returns a read/write iterator that points one past the last element in the hash map.
    iterator end() {
        return values_.end();
    }

    /// Returns a read-only (constant) iterator that points to the first element in the hash map.
    const_iterator begin() const {
        return values_.begin();
    }

    /// Returns a read-only (constant) iterator that points one past the last element in the hash map.
    const_iterator end() const {
        return values_.end();
    }

    /// Returns function used to hash the keys.
    Hasher hash_function() const {
        return hasher_;
    }

private:
    /// Information stored in a slot.
    struct Data {
        iterator value;
        bool used;
        bool deleted;
        SizeType rev_pos;
        SizeType next;
        Data() : used(false), deleted(false), next(NONE){};
    };

    /// Returns position of the key in the table or NONE, if it is not in it.
    SizeType Find(const KeyType &key, SizeType pos) const {
        while (true) {
            if (pos == NONE) {
                return NONE;
            }
            if (data_[pos].used) {
                if (data_[pos].value->first == key) {
                    return pos;
                }
            } else if (!data_[pos].deleted) {
                return NONE;
            }
            /// Using link.
            pos = data_[pos].next;
        }
    }

    /// Inserts the key that doesn't exist and returns it position in the table.
    SizeType Insert(const ValueType &value, SizeType pos) {
        /// If load factor is more than 0.5, then rehash the table.
        if ((element_count_ << 1ull) > primary_size_) {
            Rehash(primary_size_ << 1ull);
            pos = hasher_(value.first) % primary_size_;
        }

        if (data_[pos].used) {
            SizeType distance = 0;
            while (data_[pos].next != NONE && !data_[pos].deleted) {
                pos = data_[pos].next;
                ++distance;
            }
            if (data_[pos].used) {
                SizeType next_free = start_pos_;
                while (data_[next_free].used) {
                    if (next_free == 0) {
                        next_free = primary_size_ + cellar_size_ - 1;
                    } else {
                        --next_free;
                    }
                    ++distance;
                    /// If distance is more than max lookups, then immediately rehash the table. Load factor should be
                    /// more than 0.25 in case of a bad hash function.
                    if ((element_count_ << 2ull) > primary_size_ && distance > GetMaxLookups(primary_size_)) {
                        Rehash(primary_size_ << 1ull);
                        return Insert(value, hasher_(value.first) % primary_size_);
                    }
                }
                start_pos_ = next_free;
                data_[pos].next = next_free;
                pos = next_free;
            }
        }

        values_.push_back(value);
        data_[pos].value = (--values_.end());
        data_[pos].used = true;
        data_[pos].deleted = false;
        data_[pos].rev_pos = positions_.size();
        positions_.push_back(pos);
        ++element_count_;

        return pos;
    }

    /// Returns the next prime number from PRIMES.
    SizeType NextPrime(SizeType value) const {
        return *std::lower_bound(std::begin(PRIMES) + 1, std::end(PRIMES) - 1, value);
    }

    /// Rebuilds the table so that the primary_size_ is at least n.
    void Rehash(SizeType n) {
        element_count_ = 0;
        primary_size_ = NextPrime(n);
        cellar_size_ = primary_size_ * B + 1;
        start_pos_ = primary_size_ + cellar_size_ - 1;
        data_.assign(primary_size_ + cellar_size_, Data());
        if (!values_.empty()) {
            std::list<ValueType> values_copy(values_);
            values_.clear();
            positions_.clear();
            for (const auto &value : values_copy) {
                Insert(value, hasher_(value.first) % primary_size_);
            }
        }
    }

    /// Private fields:

    /// Values contained in the table.
    std::list<ValueType> values_;
    /// Positions of the values in the table.
    std::vector<SizeType> positions_;
    /// Slots of the table.
    std::vector<Data> data_;
    /// Number of elements in the table.
    SizeType element_count_;
    /// Size of the addressable part. Slots to which keys can hash to.
    SizeType primary_size_;
    /// Size of the cellar. Slots used when dealing with collisions.
    SizeType cellar_size_;
    /// Start position for searching slot to insert. Common it is always (primary_size_ + cellar_size_ - 1). Here it is
    /// recalculated after insertion.
    SizeType start_pos_;
    /// Function used to hash the keys.
    Hasher hasher_;

    /// NONE is means there is no link to the next element in chain.
    static constexpr SizeType NONE = -1;
    /// Let (cellar_size_ = B * primary_size_). The article says that this is the optimal value.
    static constexpr float B = 7 / 43.;
    /// Prime numbers for grow policy. The last is about 2^64.
    // clang-format off
    static constexpr SizeType PRIMES[] = {
        2ull, 3ull, 5ull, 7ull, 11ull, 13ull, 17ull, 23ull, 29ull, 37ull, 47ull,
        59ull, 73ull, 97ull, 127ull, 151ull, 197ull, 251ull, 313ull, 397ull,
        499ull, 631ull, 797ull, 1009ull, 1259ull, 1597ull, 2011ull, 2539ull,
        3203ull, 4027ull, 5087ull, 6421ull, 8089ull, 10193ull, 12853ull, 16193ull,
        20399ull, 25717ull, 32401ull, 40823ull, 51437ull, 64811ull, 81649ull,
        102877ull, 129607ull, 163307ull, 205759ull, 259229ull, 326617ull,
        411527ull, 518509ull, 653267ull, 823117ull, 1037059ull, 1306601ull,
        1646237ull, 2074129ull, 2613229ull, 3292489ull, 4148279ull, 5226491ull,
        6584983ull, 8296553ull, 10453007ull, 13169977ull, 16593127ull, 20906033ull,
        26339969ull, 33186281ull, 41812097ull, 52679969ull, 66372617ull,
        83624237ull, 105359939ull, 132745199ull, 167248483ull, 210719881ull,
        265490441ull, 334496971ull, 421439783ull, 530980861ull, 668993977ull,
        842879579ull, 1061961721ull, 1337987929ull, 1685759167ull, 2123923447ull,
        2675975881ull, 3371518343ull, 4247846927ull, 5351951779ull, 6743036717ull,
        8495693897ull, 10703903591ull, 13486073473ull, 16991387857ull,
        21407807219ull, 26972146961ull, 33982775741ull, 42815614441ull,
        53944293929ull, 67965551447ull, 85631228929ull, 107888587883ull,
        135931102921ull, 171262457903ull, 215777175787ull, 271862205833ull,
        342524915839ull, 431554351609ull, 543724411781ull, 685049831731ull,
        863108703229ull, 1087448823553ull, 1370099663459ull, 1726217406467ull,
        2174897647073ull, 2740199326961ull, 3452434812973ull, 4349795294267ull,
        5480398654009ull, 6904869625999ull, 8699590588571ull, 10960797308051ull,
        13809739252051ull, 17399181177241ull, 21921594616111ull, 27619478504183ull,
        34798362354533ull, 43843189232363ull, 55238957008387ull, 69596724709081ull,
        87686378464759ull, 110477914016779ull, 139193449418173ull,
        175372756929481ull, 220955828033581ull, 278386898836457ull,
        350745513859007ull, 441911656067171ull, 556773797672909ull,
        701491027718027ull, 883823312134381ull, 1113547595345903ull,
        1402982055436147ull, 1767646624268779ull, 2227095190691797ull,
        2805964110872297ull, 3535293248537579ull, 4454190381383713ull,
        5611928221744609ull, 7070586497075177ull, 8908380762767489ull,
        11223856443489329ull, 14141172994150357ull, 17816761525534927ull,
        22447712886978529ull, 28282345988300791ull, 35633523051069991ull,
        44895425773957261ull, 56564691976601587ull, 71267046102139967ull,
        89790851547914507ull, 113129383953203213ull, 142534092204280003ull,
        179581703095829107ull, 226258767906406483ull, 285068184408560057ull,
        359163406191658253ull, 452517535812813007ull, 570136368817120201ull,
        718326812383316683ull, 905035071625626043ull, 1140272737634240411ull,
        1436653624766633509ull, 1810070143251252131ull, 2280545475268481167ull,
        2873307249533267101ull, 3620140286502504283ull, 4561090950536962147ull,
        5746614499066534157ull, 7240280573005008577ull, 9122181901073924329ull,
        11493228998133068689ull, 14480561146010017169ull, 18446744073709551557ull
    };
    // clang-format on

    /// Max lookups is log2 of primary_size_.
    inline static SizeType GetMaxLookups(SizeType primary_size) {
        switch (primary_size) {
            case 17llu:
                return 5;
            case 23llu:
                return 5;
            case 29llu:
                return 5;
            case 37llu:
                return 6;
            case 47llu:
                return 6;
            case 59llu:
                return 6;
            case 73llu:
                return 7;
            case 97llu:
                return 7;
            case 127llu:
                return 7;
            case 151llu:
                return 8;
            case 197llu:
                return 8;
            case 251llu:
                return 8;
            case 313llu:
                return 9;
            case 397llu:
                return 9;
            case 499llu:
                return 9;
            case 631llu:
                return 10;
            case 797llu:
                return 10;
            case 1009llu:
                return 10;
            case 1259llu:
                return 11;
            case 1597llu:
                return 11;
            case 2011llu:
                return 11;
            case 2539llu:
                return 12;
            case 3203llu:
                return 12;
            case 4027llu:
                return 12;
            case 5087llu:
                return 13;
            case 6421llu:
                return 13;
            case 8089llu:
                return 13;
            case 10193llu:
                return 14;
            case 12853llu:
                return 14;
            case 16193llu:
                return 14;
            case 20399llu:
                return 15;
            case 25717llu:
                return 15;
            case 32401llu:
                return 15;
            case 40823llu:
                return 16;
            case 51437llu:
                return 16;
            case 64811llu:
                return 16;
            case 81649llu:
                return 17;
            case 102877llu:
                return 17;
            case 129607llu:
                return 17;
            case 163307llu:
                return 18;
            case 205759llu:
                return 18;
            case 259229llu:
                return 18;
            case 326617llu:
                return 19;
            case 411527llu:
                return 19;
            case 518509llu:
                return 19;
            case 653267llu:
                return 20;
            case 823117llu:
                return 20;
            case 1037059llu:
                return 20;
            case 1306601llu:
                return 21;
            case 1646237llu:
                return 21;
            case 2074129llu:
                return 21;
            case 2613229llu:
                return 22;
            case 3292489llu:
                return 22;
            case 4148279llu:
                return 22;
            case 5226491llu:
                return 23;
            case 6584983llu:
                return 23;
            case 8296553llu:
                return 23;
            case 10453007llu:
                return 24;
            case 13169977llu:
                return 24;
            case 16593127llu:
                return 24;
            case 20906033llu:
                return 25;
            case 26339969llu:
                return 25;
            case 33186281llu:
                return 25;
            case 41812097llu:
                return 26;
            case 52679969llu:
                return 26;
            case 66372617llu:
                return 26;
            case 83624237llu:
                return 27;
            case 105359939llu:
                return 27;
            case 132745199llu:
                return 27;
            case 167248483llu:
                return 28;
            case 210719881llu:
                return 28;
            case 265490441llu:
                return 28;
            case 334496971llu:
                return 29;
            case 421439783llu:
                return 29;
            case 530980861llu:
                return 29;
            case 668993977llu:
                return 30;
            case 842879579llu:
                return 30;
            case 1061961721llu:
                return 30;
            case 1337987929llu:
                return 31;
            case 1685759167llu:
                return 31;
            case 2123923447llu:
                return 31;
            case 2675975881llu:
                return 32;
            case 3371518343llu:
                return 32;
            case 4247846927llu:
                return 32;
            case 5351951779llu:
                return 33;
            case 6743036717llu:
                return 33;
            case 8495693897llu:
                return 33;
            case 10703903591llu:
                return 34;
            case 13486073473llu:
                return 34;
            case 16991387857llu:
                return 34;
            case 21407807219llu:
                return 35;
            case 26972146961llu:
                return 35;
            case 33982775741llu:
                return 35;
            case 42815614441llu:
                return 36;
            case 53944293929llu:
                return 36;
            case 67965551447llu:
                return 36;
            case 85631228929llu:
                return 37;
            case 107888587883llu:
                return 37;
            case 135931102921llu:
                return 37;
            case 171262457903llu:
                return 38;
            case 215777175787llu:
                return 38;
            case 271862205833llu:
                return 38;
            case 342524915839llu:
                return 39;
            case 431554351609llu:
                return 39;
            case 543724411781llu:
                return 39;
            case 685049831731llu:
                return 40;
            case 863108703229llu:
                return 40;
            case 1087448823553llu:
                return 40;
            case 1370099663459llu:
                return 41;
            case 1726217406467llu:
                return 41;
            case 2174897647073llu:
                return 41;
            case 2740199326961llu:
                return 42;
            case 3452434812973llu:
                return 42;
            case 4349795294267llu:
                return 42;
            case 5480398654009llu:
                return 43;
            case 6904869625999llu:
                return 43;
            case 8699590588571llu:
                return 43;
            case 10960797308051llu:
                return 44;
            case 13809739252051llu:
                return 44;
            case 17399181177241llu:
                return 44;
            case 21921594616111llu:
                return 45;
            case 27619478504183llu:
                return 45;
            case 34798362354533llu:
                return 45;
            case 43843189232363llu:
                return 46;
            case 55238957008387llu:
                return 46;
            case 69596724709081llu:
                return 46;
            case 87686378464759llu:
                return 47;
            case 110477914016779llu:
                return 47;
            case 139193449418173llu:
                return 47;
            case 175372756929481llu:
                return 48;
            case 220955828033581llu:
                return 48;
            case 278386898836457llu:
                return 48;
            case 350745513859007llu:
                return 49;
            case 441911656067171llu:
                return 49;
            case 556773797672909llu:
                return 49;
            case 701491027718027llu:
                return 50;
            case 883823312134381llu:
                return 50;
            case 1113547595345903llu:
                return 50;
            case 1402982055436147llu:
                return 51;
            case 1767646624268779llu:
                return 51;
            case 2227095190691797llu:
                return 51;
            case 2805964110872297llu:
                return 52;
            case 3535293248537579llu:
                return 52;
            case 4454190381383713llu:
                return 52;
            case 5611928221744609llu:
                return 53;
            case 7070586497075177llu:
                return 53;
            case 8908380762767489llu:
                return 53;
            case 11223856443489329llu:
                return 54;
            case 14141172994150357llu:
                return 54;
            case 17816761525534927llu:
                return 54;
            case 22447712886978529llu:
                return 55;
            case 28282345988300791llu:
                return 55;
            case 35633523051069991llu:
                return 55;
            case 44895425773957261llu:
                return 56;
            case 56564691976601587llu:
                return 56;
            case 71267046102139967llu:
                return 56;
            case 89790851547914507llu:
                return 57;
            case 113129383953203213llu:
                return 57;
            case 142534092204280003llu:
                return 57;
            case 179581703095829107llu:
                return 58;
            case 226258767906406483llu:
                return 58;
            case 285068184408560057llu:
                return 58;
            case 359163406191658253llu:
                return 59;
            case 452517535812813007llu:
                return 59;
            case 570136368817120201llu:
                return 59;
            case 718326812383316683llu:
                return 60;
            case 905035071625626043llu:
                return 60;
            case 1140272737634240411llu:
                return 60;
            case 1436653624766633509llu:
                return 61;
            case 1810070143251252131llu:
                return 61;
            case 2280545475268481167llu:
                return 61;
            case 2873307249533267101llu:
                return 62;
            case 3620140286502504283llu:
                return 62;
            case 4561090950536962147llu:
                return 62;
            case 5746614499066534157llu:
                return 63;
            case 7240280573005008577llu:
                return 63;
            case 9122181901073924329llu:
                return 63;
            case 11493228998133068689llu:
                return 64;
            case 14480561146010017169llu:
                return 64;
            case 18446744073709551557llu:
                return 64;
            default:
                return 4;
        }
    }
};
