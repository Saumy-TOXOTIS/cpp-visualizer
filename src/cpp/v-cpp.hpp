#ifndef V_CPP_HPP
#define V_CPP_HPP

// ########## C++ Visualization Framework - By Saumy Tiwari ##########
// This is the header-only library file. Include this in your project to use the framework.

#include "libraries.hpp"
#include <emscripten/bind.h>
#include "include/nlohmann/json.hpp"

using namespace std;
using json = nlohmann::json;

// --- The Core Engine: The Visualizer ---
class VizEngine
{
public:
    json history;
    map<string, json> object_states;

    // NEW: Reset method to clear the state for a new run
    void reset() {
        history.clear();
        object_states.clear();
    }

    void log_frame(const string &message) { history.push_back({{"message", message}, {"objects", object_states}}); }

    // --- NEW: Universal JSON serialization helper using C++17 Fold Expressions ---
    // Helper to apply a function to each element of a tuple
    template <typename Tuple, typename Func, size_t... Is>
    void for_each_in_tuple(Tuple &&tuple, Func &&func, index_sequence<Is...>)
    {
        (func(get<Is>(tuple)), ...);
    }

    template <typename T>
    json to_json_recursive(const T &data)
    {
        // Base case: for simple scalar types like int, double, char, string, bool
        if constexpr (is_scalar<T>::value || is_same_v<T, string>)
        {
            return data;
        }
        // For vector-like containers (vector, list, deque, set, multiset, etc.)
        else if constexpr (requires { data.begin(); data.end(); } && !is_same_v<T, map<int, int>> && !is_same_v<T, unordered_map<int, int>>)
        {
            json j_arr = json::array();
            for (const auto &item : data)
            {
                j_arr.push_back(to_json_recursive(item));
            }
            return j_arr;
        }
        // For map-like containers
        else if constexpr (requires { data.begin()->first; data.begin()->second; })
        {
            json j_map = json::array();
            for (const auto &pair : data)
            {
                j_map.push_back({{"key", to_json_recursive(pair.first)}, {"value", to_json_recursive(pair.second)}});
            }
            return j_map;
        }
        // For pair
        else if constexpr (requires { data.first; data.second; })
        {
            return {to_json_recursive(data.first), to_json_recursive(data.second)};
        }
        // THE UPGRADE: For ANY size tuple
        else if constexpr (requires { tuple_size<T>::value; })
        {
            json j_arr = json::array();
            for_each_in_tuple(data, [&](const auto &element)
                              { j_arr.push_back(to_json_recursive(element)); }, make_index_sequence<tuple_size<T>::value>{});
            return j_arr;
        }
        // Fallback for unknown types
        return "unsupported_type";
    }

    template <typename T>
    void update_state(const string &name, const string &type, const T &data, const map<string, string> &highlights = {})
    {
        json j_data;

        // THE UPGRADE: Templatized Stack/Queue/PQ serialization
        if constexpr (requires { T().top(); T().pop(); })
        { // Stack or PQ
            T temp = data;
            vector<json> temp_vec;
            while (!temp.empty())
            {
                temp_vec.push_back(to_json_recursive(temp.top()));
                temp.pop();
            }
            j_data = temp_vec;
        }
        else if constexpr (requires { T().front(); T().pop(); })
        { // Queue
            T temp = data;
            vector<json> temp_vec;
            while (!temp.empty())
            {
                temp_vec.push_back(to_json_recursive(temp.front()));
                temp.pop();
            }
            j_data = temp_vec;
        }
        else
        {
            // The update function is now incredibly simple for everything else!
            j_data = to_json_recursive(data);
        }

        object_states[name] = {{"type", type}, {"data", j_data}, {"highlights", highlights}};
    }
};
inline VizEngine viz;

// Base class for all visualizable objects
class v_base
{
public:
    string v_name;
    string v_type;
    v_base(string name, string type) : v_name(name), v_type(type) {}
};

// The Proxy: The Heart of the Operator Overloading Magic
// --- The Proxy System: The Heart of the Magic ---

// Forward declarations for our proxy classes
template <typename Parent, typename KeyType>
class v_proxy;
template <typename Parent>
class v_proxy_2d_row;
template <typename Parent>
class v_proxy_2d_cell;

// The 1D Proxy (for vectors, maps, deques, etc.)
template <typename Parent, typename KeyType>
class v_proxy
{
private:
    Parent *parent;
    KeyType key;

public:
    v_proxy(Parent *p, KeyType k) : parent(p), key(k) {}

    using is_a_proxy = void; // This is a marker type to indicate this is a proxy

    // Universal Read Operator: `auto x = my_vec[i];`
    // It can now return any type (int, string, double...)
    template <typename T = typename Parent::DataType::value_type>
    operator T() const
    {
        // Use stringstream for a universal, non-ambiguous to-string conversion.
        stringstream ss;
        ss << key;
        string h_key = ss.str(); // Simplified for visualization key
        viz.update_state(parent->v_name, parent->v_type, parent->data, {{h_key, "read"}});
        viz.log_frame("Reading from " + parent->v_name + " at key/index " + h_key);

        if constexpr (requires { typename Parent::DataType::key_type; })
        { // Map-like
            return parent->data.count(key) ? parent->data.at(key) : T{};
        }
        else
        { // Vector-like
            return parent->data[key];
        }
    }

    // Universal Write Operator: `my_vec[i] = "Saumy";`
    template <typename T>
    v_proxy &operator=(const T &value)
    {
        parent->data[key] = value;
        stringstream ss;
        ss << key;
        string h_key = ss.str(); // Simplified for visualization key
        viz.update_state(parent->v_name, parent->v_type, parent->data, {{h_key, "write"}});
        // We can't easily stringify generic types here, so we keep the message simple.
        viz.log_frame("Writing to " + parent->v_name + " at key/index " + h_key);
        return *this;
    }
};

// The 2D Proxy for Matrices (now also fully generic)
template <typename Parent>
class v_proxy_2d_row
{
private:
    Parent *parent;
    int row;

public:
    v_proxy_2d_row(Parent *p, int r) : parent(p), row(r) {}
    v_proxy_2d_cell<Parent> operator[](int col)
    {
        return v_proxy_2d_cell<Parent>(parent, row, col);
    }
    // --- THE NEW FEATURE: Assigning a whole row at once ---
    template <typename T>
    v_proxy_2d_row &operator=(const std::vector<T> &new_row_values)
    {
        // Step 1: Replace the data in the specified row of the parent matrix.
        parent->data[row] = new_row_values;

        // Step 2: Update the visual state. We will highlight every cell in the changed row.
        map<string, string> highlights;
        for (size_t col = 0; col < new_row_values.size(); ++col)
        {
            highlights[to_string(row) + "-" + to_string(col)] = "write";
        }
        viz.update_state(parent->v_name, parent->v_type, parent->data, highlights);

        // Step 3: Log a single, clean frame for this action.
        viz.log_frame("Assigned new values to row " + to_string(row) + " of '" + parent->v_name + "'.");

        // Step 4: Return a reference to this proxy object.
        return *this;
    }
};

template <typename Parent>
class v_proxy_2d_cell
{
private:
    Parent *parent;
    int row, col;

public:
    v_proxy_2d_cell(Parent *p, int r, int c) : parent(p), row(r), col(c) {}

    // Universal Read Operator for Matrix Cell
    template <typename T = typename Parent::DataType::value_type::value_type>
    operator T() const
    {
        string h_key = to_string(row) + "-" + to_string(col);
        viz.update_state(parent->v_name, parent->v_type, parent->data, {{h_key, "read"}});
        viz.log_frame("Read from " + parent->v_name + "[" + to_string(row) + "][" + to_string(col) + "]");
        return parent->data[row][col];
    }

    // Universal Write Operator for Matrix Cell
    template <typename T>
    v_proxy_2d_cell &operator=(const T &value)
    {
        parent->data[row][col] = value;
        string h_key = to_string(row) + "-" + to_string(col);
        viz.update_state(parent->v_name, parent->v_type, parent->data, {{h_key, "write"}});
        viz.log_frame("Write to " + parent->v_name + "[" + to_string(row) + "][" + to_string(col) + "]");
        return *this;
    }
};

// --- A helper proxy for getting/setting tuple/pair elements ---
template <typename Parent, size_t Index>
class v_get_proxy
{
private:
    Parent *parent;

public:
    v_get_proxy(Parent *p) : parent(p) {}

    // Reading from the element (e.g., int x = v_get<0>(my_pair);)
    operator auto() const
    {
        viz.update_state(parent->v_name, parent->v_type, parent->data, {{to_string(Index), "read"}});
        viz.log_frame("Reading element " + to_string(Index) + " from '" + parent->v_name + "'.");
        return get<Index>(parent->data);
    }

    // Writing to the element (e.g., v_get<0>(my_pair) = 100;)
    template <typename T>
    v_get_proxy &operator=(const T &value)
    {
        get<Index>(parent->data) = value;
        viz.update_state(parent->v_name, parent->v_type, parent->data, {{to_string(Index), "write"}});
        viz.log_frame("Writing to element " + to_string(Index) + " of '" + parent->v_name + "'.");
        return *this;
    }
};

// PAIR
template <typename T1, typename T2>
class v_pair : public v_base
{
public:
    using DataType = pair<T1, T2>;
    pair<T1, T2> data;
    // --- Constructor 1: Create a default-initialized pair ---
    v_pair(string n) : v_base(n, "pair"), data()
    {
        viz.update_state(v_name, v_type, data);
        viz.log_frame("Created default-initialized pair '" + v_name + "'.");
    }

    // --- Constructor 2: Create from a standard C++ pair ---
    v_pair(string n, const std::pair<T1, T2> &iv) : v_base(n, "pair"), data(iv)
    {
        viz.update_state(v_name, v_type, data);
        viz.log_frame("Created pair '" + v_name + "' from initial data.");
    }

    // --- THE NEW FEATURE: Assignment from a std::pair ---
    v_pair<T1, T2> &operator=(const std::pair<T1, T2> &new_values)
    {
        this->data = new_values;
        // Highlight both elements of the pair on write
        viz.update_state(v_name, v_type, this->data, {{"0", "write"}, {"1", "write"}});
        viz.log_frame("Assigned new contents to pair '" + v_name + "'.");
        return *this;
    }
};

// TUPLE (now fully generic)
template <typename... Types>
class v_tuple : public v_base
{
public:
    using DataType = tuple<Types...>;
    DataType data;

    // --- Constructor 1: Create a default-initialized tuple ---
    v_tuple(string n) : v_base(n, "tuple"), data()
    {
        viz.update_state(v_name, v_type, data);
        viz.log_frame("Created default-initialized tuple '" + v_name + "'.");
    }

    // --- Constructor 2: Create from a standard C++ tuple ---
    v_tuple(string n, const DataType &iv) : v_base(n, "tuple"), data(iv)
    {
        viz.update_state(v_name, v_type, data);
        viz.log_frame("Created tuple '" + v_name + "' from initial data.");
    }

    // --- THE NEW FEATURE: Assignment from a std::tuple ---
    v_tuple<Types...> &operator=(const DataType &new_values)
    {
        this->data = new_values;

        // Highlight all elements of the tuple on write
        map<string, string> highlights;
        [&]<size_t... Is>(index_sequence<Is...>)
        {
            ((highlights[to_string(Is)] = "write"), ...);
        }(index_sequence_for<Types...>{});

        viz.update_state(v_name, v_type, this->data, highlights);
        viz.log_frame("Assigned new contents to tuple '" + v_name + "'.");
        return *this;
    }
};

// --- The Magic: Visualizable std::get for v_pair and v_tuple ---

// For v_pair
template <size_t I, typename T1, typename T2>
auto v_get(v_pair<T1, T2> &p)
{
    return v_get_proxy<v_pair<T1, T2>, I>(&p);
}

// For v_tuple
template <size_t I, typename... Types>
auto v_get(v_tuple<Types...> &t)
{
    return v_get_proxy<v_tuple<Types...>, I>(&t);
}

template <typename T>
class v_scalar : public v_base
{
public:
    using DataType = T;
    T data;
    // --- Constructor 1 (NEW): Create a default-initialized scalar ---
    v_scalar(string n) : v_base(n, is_same_v<T, string> ? "string" : (is_same_v<T, bool> ? "bool" : "scalar")),
                         data() // Default-initializes the data (0 for int, false for bool, "" for string, etc.)
    {
        viz.update_state(v_name, v_type, data);

        string value_str;
        if constexpr (is_same_v<T, string>)
        {
            value_str = "\"" + data + "\""; // Put quotes around empty string
        }
        else
        {
            value_str = std::to_string(data);
        }
        viz.log_frame("Created default-initialized " + v_type + " '" + v_name + "' with value " + value_str);
    }

    // --- Constructor 2: Create with an initial value ---
    v_scalar(string n, T iv) : v_base(n, is_same_v<T, string> ? "string" : (is_same_v<T, bool> ? "bool" : "scalar")),
                               data(iv)
    {
        viz.update_state(v_name, v_type, data);

        string value_str;
        if constexpr (is_same_v<T, string>)
        {
            value_str = "\"" + data + "\""; // Put quotes around the string for clarity
        }
        else
        {
            value_str = std::to_string(data);
        }
        viz.log_frame("Created " + v_type + " '" + v_name + "' with value " + value_str);
    }

    // --- The Assignment Operator ---
    v_scalar<T> &operator=(T v)
    {
        data = v;
        viz.update_state(v_name, v_type, data, {{"0", "write"}});

        string value_str;
        if constexpr (is_same_v<T, string>)
        {
            value_str = "\"" + data + "\"";
        }
        else
        {
            value_str = std::to_string(data);
        }
        viz.log_frame("Set '" + v_name + "' = " + value_str);

        return *this;
    }
    operator T() const
    {
        viz.update_state(v_name, v_type, data, {{"0", "read"}});
        viz.log_frame("Read " + v_name);
        return data;
    }
};
template <typename T>
class v_vector : public v_base
{
public:
    using DataType = vector<T>;
    vector<T> data;
    v_vector(string n, int size) : v_base(n, "vector"), data(size)
    {
        viz.update_state(v_name, v_type, data);
        viz.log_frame("Created vector '" + v_name + "' with size " + to_string(size));
    }
    // --- Constructor 2: Initialize from an existing std::vector ---
    v_vector(string n, const std::vector<T> &initial_values) : v_base(n, "vector"), data(initial_values)
    {
        viz.update_state(v_name, v_type, data);
        viz.log_frame("Created vector '" + v_name + "' from initial data.");
    }

    // --- THE NEW FEATURE: Assignment from a std::vector ---
    v_vector<T> &operator=(const std::vector<T> &new_values)
    {
        // Step 1: Replace the internal data with the new data.
        this->data = new_values;

        // Step 2: Update the visual state to show the new data.
        // We will highlight the entire vector to show it was changed.
        viz.update_state(v_name, v_type, this->data, {{"0", "write"}}); // Note: highlighting '0' is a simple way to pulse the object.

        // Step 3: Log a frame to capture this change.
        viz.log_frame("Assigned new contents to vector '" + v_name + "'.");

        // Step 4: Return a reference to this object, as is standard for operator=.
        return *this;
    }
    v_proxy<v_vector, int> operator[](int i)
    {
        return v_proxy<v_vector, int>(this, i);
    }
    void push_back(T v)
    {
        data.push_back(v);
        viz.update_state(v_name, v_type, data, {{to_string(data.size() - 1), "write"}});
        string value_str;
        if constexpr (is_same_v<T, string>)
        {
            value_str = v;
        }
        else
        {
            value_str = std::to_string(v);
        }
        viz.log_frame("Pushed " + value_str + " to '" + v_name + "'");
    }
    size_t size() const { return data.size(); }
};
template <typename T>
class v_list : public v_base
{
public:
    using DataType = list<T>;
    list<T> data;

    // --- Constructor 1: Create an empty list ---
    v_list(string n) : v_base(n, "list")
    {
        viz.update_state(v_name, v_type, data);
        viz.log_frame("Created empty list '" + v_name + "'.");
    }

    // --- Constructor 2: Create a list of a specific size (with default values) ---
    v_list(string n, int size) : v_base(n, "list"), data(size)
    {
        viz.update_state(v_name, v_type, data);
        viz.log_frame("Created list '" + v_name + "' with size " + to_string(size));
    }

    // --- Constructor 3: Create from a standard C++ list ---
    v_list(string n, const std::list<T> &initial_values) : v_base(n, "list"), data(initial_values)
    {
        viz.update_state(v_name, v_type, data);
        viz.log_frame("Created list '" + v_name + "' from initial data.");
    }

    // --- Assignment Operator from a std::list ---
    v_list<T> &operator=(const std::list<T> &new_values)
    {
        this->data = new_values;
        viz.update_state(v_name, v_type, this->data);
        viz.log_frame("Assigned new contents to list '" + v_name + "'.");
        return *this;
    }

    // --- Visualizable Member Functions ---
    void push_back(const T &v)
    {
        data.push_back(v);
        viz.update_state(v_name, v_type, data);
        viz.log_frame("Pushed back " + to_string(v) + " to '" + v_name + "'.");
    }

    void pop_back()
    {
        if (data.empty())
            return;
        data.pop_back();
        viz.update_state(v_name, v_type, data);
        viz.log_frame("Popped back from '" + v_name + "'.");
    }

    void push_front(const T &v)
    {
        data.push_front(v);
        viz.update_state(v_name, v_type, data);
        viz.log_frame("Pushed front " + to_string(v) + " to '" + v_name + "'.");
    }

    void pop_front()
    {
        if (data.empty())
            return;
        data.pop_front();
        viz.update_state(v_name, v_type, data);
        viz.log_frame("Popped front from '" + v_name + "'.");
    }

    void clear()
    {
        data.clear();
        viz.update_state(v_name, v_type, data);
        viz.log_frame("Cleared list '" + v_name + "'.");
    }

    // --- Utility Functions ---
    size_t size() const { return data.size(); }
    bool empty() const { return data.empty(); }
};
template <typename T>
class v_stack : public v_base
{
public:
    // The internal data type is now a standard stack
    using DataType = std::stack<T>;
    DataType data;

    // --- Constructor 1: Create an empty stack ---
    v_stack(string n) : v_base(n, "stack")
    {
        viz.update_state(v_name, v_type, data);
        viz.log_frame("Created empty stack '" + v_name + "'.");
    }

    // --- Constructor 2 (THE NEW GENERIC ONE): Create from ANY compatible container ---
    template <typename Container>
    v_stack(string n, const Container &initial_values) : v_base(n, "stack"),
                                                         data(initial_values) // std::stack's constructor can take a container like vector or deque
    {
        viz.update_state(v_name, v_type, data);
        viz.log_frame("Created stack '" + v_name + "' from initial data.");
    }

    // --- THE NEW FEATURE: Assignment from a compatible container ---
    template <typename Container>
    v_stack<T> &operator=(const Container &new_values)
    {
        // Step 1: Create a new temporary stack from the container.
        // This is the cleanest way to replace the contents.
        this->data = DataType(new_values);

        // Step 2: Update the visual state and log the change.
        viz.update_state(v_name, v_type, this->data);
        viz.log_frame("Assigned new contents to stack '" + v_name + "'.");

        return *this;
    }

    // --- Visualizable Member Functions ---
    void push(T v)
    {
        data.push(v);
        viz.update_state(v_name, v_type, data, {{"top", "write"}});
        viz.log_frame("Pushed " + to_string(v) + " onto stack '" + v_name + "'.");
    }

    T top()
    {
        T v = data.top();
        viz.update_state(v_name, v_type, data, {{"top", "read"}});
        viz.log_frame("Read top element (" + to_string(v) + ") from stack '" + v_name + "'.");
        return v;
    }

    void pop()
    {
        if (data.empty())
            return;
        T v = data.top();
        data.pop();
        viz.update_state(v_name, v_type, data);
        viz.log_frame("Popped element (" + to_string(v) + ") from stack '" + v_name + "'.");
    }

    bool empty() const { return data.empty(); }
};
template <typename T>
class v_queue : public v_base
{
public:
    using DataType = std::queue<T>;
    DataType data;

    // --- Constructor 1: Create an empty queue ---
    v_queue(string n) : v_base(n, "queue")
    {
        viz.update_state(v_name, v_type, data);
        viz.log_frame("Created empty queue '" + v_name + "'.");
    }

    // --- Constructor 2 (THE NEW GENERIC ONE): Create from ANY compatible container ---
    template <typename Container>
    v_queue(string n, const Container &initial_values) : v_base(n, "queue"),
                                                         data(initial_values) // std::queue's constructor also takes a container
    {
        viz.update_state(v_name, v_type, data);
        viz.log_frame("Created queue '" + v_name + "' from initial data.");
    }

    // --- THE NEW FEATURE: Assignment from a compatible container ---
    template <typename Container>
    v_queue<T> &operator=(const Container &new_values)
    {
        // Step 1: Create a new temporary queue from the container to replace the contents.
        this->data = DataType(new_values);

        // Step 2: Update the visual state and log the change.
        viz.update_state(v_name, v_type, this->data);
        viz.log_frame("Assigned new contents to queue '" + v_name + "'.");

        return *this;
    }

    // --- Visualizable Member Functions ---
    void push(T v)
    {
        data.push(v);
        viz.update_state(v_name, v_type, data, {{"back", "write"}});
        viz.log_frame("Pushed " + to_string(v) + " to queue '" + v_name + "'.");
    }

    T front()
    {
        T v = data.front();
        viz.update_state(v_name, v_type, data, {{"front", "read"}});
        viz.log_frame("Read front element (" + to_string(v) + ") from queue '" + v_name + "'.");
        return v;
    }

    void pop()
    {
        if (data.empty())
            return;
        T v = data.front();
        data.pop();
        viz.update_state(v_name, v_type, data);
        viz.log_frame("Popped element (" + to_string(v) + ") from queue '" + v_name + "'.");
    }

    bool empty() const
    {
        return data.empty();
    }
};
template <typename T>
class v_deque : public v_base
{
public:
    using DataType = deque<T>;
    deque<T> data;

    // --- Constructor 1: Create an empty deque ---
    v_deque(string n) : v_base(n, "deque")
    {
        viz.update_state(v_name, v_type, data);
        viz.log_frame("Created empty deque '" + v_name + "'.");
    }

    // --- Constructor 2: Create from a standard C++ deque ---
    v_deque(string n, const std::deque<T> &initial_values) : v_base(n, "deque"), data(initial_values)
    {
        viz.update_state(v_name, v_type, data);
        viz.log_frame("Created deque '" + v_name + "' from initial data.");
    }

    // --- THE NEW FEATURE: Assignment from a std::deque ---
    v_deque<T> &operator=(const std::deque<T> &new_values)
    {
        this->data = new_values;
        viz.update_state(v_name, v_type, this->data);
        viz.log_frame("Assigned new contents to deque '" + v_name + "'.");
        return *this;
    }

    // --- Visualizable Member Functions ---
    v_proxy<v_deque, int> operator[](int i) { return v_proxy<v_deque, int>(this, i); }
    size_t size() const { return data.size(); }
    bool empty() const { return data.empty(); }

    void push_back(T v)
    {
        data.push_back(v);
        viz.update_state(v_name, v_type, data, {{"back", "write"}});
        viz.log_frame("Pushed back " + to_string(v) + " to '" + v_name + "'.");
    }

    void push_front(T v)
    {
        data.push_front(v);
        viz.update_state(v_name, v_type, data, {{"front", "write"}});
        viz.log_frame("Pushed front " + to_string(v) + " to '" + v_name + "'.");
    }

    void pop_back()
    {
        if (data.empty())
            return;
        data.pop_back();
        viz.update_state(v_name, v_type, data);
        viz.log_frame("Popped back from '" + v_name + "'.");
    }

    void pop_front()
    {
        if (data.empty())
            return;
        data.pop_front();
        viz.update_state(v_name, v_type, data);
        viz.log_frame("Popped front from '" + v_name + "'.");
    }
};
template <typename T>
class v_priority_queue : public v_base
{
public:
    using DataType = priority_queue<T, std::vector<T>, std::less<T>>;
    DataType data;

    // --- Constructor 1: Create an empty priority_queue ---
    v_priority_queue(string n) : v_base(n, "priority_queue")
    {
        viz.update_state(v_name, v_type, data);
        viz.log_frame("Created empty priority_queue '" + v_name + "'.");
    }

    // --- Constructor 2 (NEW GENERIC FEATURE): Create from ANY compatible container ---
    template <typename Container>
    v_priority_queue(string n, const Container &initial_values) : v_base(n, "priority_queue"),
                                                                  // The std::priority_queue constructor automatically performs the heapify operation
                                                                  data(initial_values.begin(), initial_values.end())
    {
        viz.update_state(v_name, v_type, data);
        viz.log_frame("Created priority_queue '" + v_name + "' from initial data.");
    }

    // --- THE NEW FEATURE: Assignment from a compatible container ---
    template <typename Container>
    v_priority_queue<T> &operator=(const Container &new_values)
    {
        // Step 1: Create a new temporary priority_queue from the container.
        // The constructor of std::priority_queue handles the heapify process automatically.
        this->data = DataType(new_values.begin(), new_values.end());

        // Step 2: Update the visual state and log the change.
        viz.update_state(v_name, v_type, this->data);
        viz.log_frame("Assigned new contents to priority_queue '" + v_name + "'.");

        return *this;
    }

    // --- Visualizable Member Functions ---
    void push(T v)
    {
        data.push(v);
        viz.update_state(v_name, v_type, data, {{"top", "write"}});
        viz.log_frame("Pushed " + to_string(v) + " to priority_queue '" + v_name + "'.");
    }

    T top()
    {
        T v = data.top();
        viz.update_state(v_name, v_type, data, {{"top", "read"}});
        viz.log_frame("Read top element (" + to_string(v) + ") from priority_queue '" + v_name + "'.");
        return v;
    }

    void pop()
    {
        if (data.empty())
            return;
        T v = data.top();
        data.pop();
        viz.update_state(v_name, v_type, data);
        viz.log_frame("Popped element (" + to_string(v) + ") from priority_queue '" + v_name + "'.");
    }

    bool empty() const { return data.empty(); }
    size_t size() const { return data.size(); }
};
template <typename T>
class v_set : public v_base
{
public:
    using DataType = set<T>;
    set<T> data;
    // --- Constructor 1: Create an empty set ---
    v_set(string n) : v_base(n, "set")
    {
        viz.update_state(v_name, v_type, data);
        viz.log_frame("Created empty set '" + v_name + "'.");
    }

    // --- Constructor 2: Create from a standard C++ set ---
    v_set(string n, const std::set<T> &initial_values) : v_base(n, "set"), data(initial_values)
    {
        viz.update_state(v_name, v_type, data);
        viz.log_frame("Created set '" + v_name + "' from initial data.");
    }

    // --- THE NEW FEATURE: Assignment from a std::set ---
    v_set<T> &operator=(const std::set<T> &new_values)
    {
        this->data = new_values;
        viz.update_state(v_name, v_type, this->data); // No specific highlight, just show the new state
        viz.log_frame("Assigned new contents to set '" + v_name + "'.");
        return *this;
    }
    void insert(T v)
    {
        data.insert(v);
        viz.update_state(v_name, v_type, data, {{to_string(v), "write"}});
    }
    void erase(T v)
    {
        data.erase(v);
        viz.update_state(v_name, v_type, data, {{to_string(v), "read"}}); // Highlight the value being removed
        viz.log_frame("Erased " + to_string(v) + " from " + v_name);
    }
    bool find(T v)
    {
        bool found = data.count(v) > 0;
        viz.update_state(v_name, v_type, data, {{to_string(v), "compare"}});
        viz.log_frame("Finding " + to_string(v) + " in " + v_name);
        return found;
    }
};
template <typename T>
class v_multiset : public v_base
{
public:
    using DataType = multiset<T>;
    multiset<T> data;
    // --- Constructor 1: Create an empty multiset ---
    v_multiset(string n) : v_base(n, "multiset")
    {
        viz.update_state(v_name, v_type, data);
        viz.log_frame("Created empty multiset '" + v_name + "'.");
    }

    // --- Constructor 2: Create from a standard C++ multiset ---
    v_multiset(string n, const std::multiset<T> &initial_values) : v_base(n, "multiset"), data(initial_values)
    {
        viz.update_state(v_name, v_type, data);
        viz.log_frame("Created multiset '" + v_name + "' from initial data.");
    }

    // --- THE NEW FEATURE: Assignment from a std::multiset ---
    v_multiset<T> &operator=(const std::multiset<T> &new_values)
    {
        this->data = new_values;
        viz.update_state(v_name, v_type, this->data);
        viz.log_frame("Assigned new contents to multiset '" + v_name + "'.");
        return *this;
    }

    // --- Visualizable Member Functions ---
    void insert(T v)
    {
        data.insert(v);
        viz.update_state(v_name, v_type, data, {{to_string(v), "write"}});
        viz.log_frame("Inserted " + to_string(v) + " into '" + v_name + "'.");
    }

    void erase(T v)
    {
        if (data.count(v) > 0)
        {
            viz.update_state(v_name, v_type, data, {{to_string(v), "read"}});
            viz.log_frame("Erasing one instance of " + to_string(v) + " from '" + v_name + "'.");
            data.erase(data.find(v)); // Erase only one instance
            viz.update_state(v_name, v_type, data);
        }
    }
};
template <typename K, typename V>
class v_map : public v_base
{
public:
    using DataType = map<K, V>;
    map<K, V> data;
    // --- Constructor 1: Create an empty map ---
    v_map(string n) : v_base(n, "map")
    {
        viz.update_state(v_name, v_type, data);
        viz.log_frame("Created empty map '" + v_name + "'.");
    }

    // --- Constructor 2: Create from a standard C++ map ---
    v_map(string n, const std::map<K, V> &initial_values) : v_base(n, "map"), data(initial_values)
    {
        viz.update_state(v_name, v_type, data);
        viz.log_frame("Created map '" + v_name + "' from initial data.");
    }

    // --- THE NEW FEATURE: Assignment from a std::map ---
    v_map<K, V> &operator=(const std::map<K, V> &new_values)
    {
        this->data = new_values;
        viz.update_state(v_name, v_type, this->data);
        viz.log_frame("Assigned new contents to map '" + v_name + "'.");
        return *this;
    }
    v_proxy<v_map, K> operator[](K k) { return v_proxy<v_map, K>(this, k); }
};
template <typename K, typename V>
class v_multimap : public v_base
{
public:
    using DataType = multimap<K, V>;
    multimap<K, V> data;
    // --- Constructor 1: Create an empty multimap ---
    v_multimap(string n) : v_base(n, "multimap")
    {
        viz.update_state(v_name, v_type, data);
        viz.log_frame("Created empty multimap '" + v_name + "'.");
    }

    // --- Constructor 2: Create from a standard C++ multimap ---
    v_multimap(string n, const std::multimap<K, V> &initial_values) : v_base(n, "multimap"), data(initial_values)
    {
        viz.update_state(v_name, v_type, data);
        viz.log_frame("Created multimap '" + v_name + "' from initial data.");
    }

    // --- THE NEW FEATURE: Assignment from a std::multimap ---
    v_multimap<K, V> &operator=(const std::multimap<K, V> &new_values)
    {
        this->data = new_values;
        viz.update_state(v_name, v_type, this->data);
        viz.log_frame("Assigned new contents to multimap '" + v_name + "'.");
        return *this;
    }

    // --- Visualizable Member Functions ---
    void insert(pair<K, V> p)
    {
        data.insert(p);
        viz.update_state(v_name, v_type, data, {{to_string(p.first), "write"}});
        // A more descriptive message for multimap
        viz.log_frame("Inserted pair (" + to_string(p.first) + ", " + to_string(p.second) + ") into '" + v_name + "'.");
    }
};
template <typename T>
class v_unordered_set : public v_base
{
public:
    using DataType = unordered_set<T>;
    unordered_set<T> data;
    // --- Constructor 1: Create an empty unordered_set ---
    v_unordered_set(string n) : v_base(n, "unordered_set")
    {
        viz.update_state(v_name, v_type, data);
        viz.log_frame("Created empty unordered_set '" + v_name + "'.");
    }

    // --- Constructor 2: Create from a standard C++ unordered_set ---
    v_unordered_set(string n, const std::unordered_set<T> &initial_values) : v_base(n, "unordered_set"), data(initial_values)
    {
        viz.update_state(v_name, v_type, data);
        viz.log_frame("Created unordered_set '" + v_name + "' from initial data.");
    }

    // --- THE NEW FEATURE: Assignment from a std::unordered_set ---
    v_unordered_set<T> &operator=(const std::unordered_set<T> &new_values)
    {
        this->data = new_values;
        viz.update_state(v_name, v_type, this->data);
        viz.log_frame("Assigned new contents to unordered_set '" + v_name + "'.");
        return *this;
    }

    // --- Visualizable Member Functions ---
    void insert(T v)
    {
        data.insert(v);
        viz.update_state(v_name, v_type, data, {{to_string(v), "write"}});
        viz.log_frame("Inserted " + to_string(v) + " into '" + v_name + "'.");
    }

    void erase(T v)
    {
        if (data.count(v) > 0)
        {
            viz.update_state(v_name, v_type, data, {{to_string(v), "read"}});
            viz.log_frame("Erasing " + to_string(v) + " from '" + v_name + "'.");
            data.erase(v);
            viz.update_state(v_name, v_type, data);
        }
    }

    bool find(T v)
    {
        bool found = data.count(v) > 0;
        viz.update_state(v_name, v_type, data, {{to_string(v), "compare"}});
        viz.log_frame("Finding " + to_string(v) + " in '" + v_name + "'.");
        return found;
    }
};
template <typename T>
class v_unordered_multiset : public v_base
{
public:
    using DataType = unordered_multiset<T>;
    unordered_multiset<T> data;
    // --- Constructor 1: Create an empty unordered_multiset ---
    v_unordered_multiset(string n) : v_base(n, "unordered_multiset")
    {
        viz.update_state(v_name, v_type, data);
        viz.log_frame("Created empty unordered_multiset '" + v_name + "'.");
    }

    // --- Constructor 2: Create from a standard C++ unordered_multiset ---
    v_unordered_multiset(string n, const std::unordered_multiset<T> &initial_values) : v_base(n, "unordered_multiset"), data(initial_values)
    {
        viz.update_state(v_name, v_type, data);
        viz.log_frame("Created unordered_multiset '" + v_name + "' from initial data.");
    }

    // --- THE NEW FEATURE: Assignment from a std::unordered_multiset ---
    v_unordered_multiset<T> &operator=(const std::unordered_multiset<T> &new_values)
    {
        this->data = new_values;
        viz.update_state(v_name, v_type, this->data);
        viz.log_frame("Assigned new contents to unordered_multiset '" + v_name + "'.");
        return *this;
    }

    // --- Visualizable Member Functions ---
    void insert(T v)
    {
        data.insert(v);
        viz.update_state(v_name, v_type, data, {{to_string(v), "write"}});
        viz.log_frame("Inserted " + to_string(v) + " into '" + v_name + "'.");
    }

    void erase(T v)
    {
        if (data.count(v) > 0)
        {
            viz.update_state(v_name, v_type, data, {{to_string(v), "read"}});
            viz.log_frame("Erasing one instance of " + to_string(v) + " from '" + v_name + "'.");
            data.erase(data.find(v)); // Erase only one instance
            viz.update_state(v_name, v_type, data);
        }
    }
};
template <typename K, typename V>
class v_unordered_map : public v_base
{
public:
    using DataType = unordered_map<K, V>;
    unordered_map<K, V> data;
    // --- Constructor 1: Create an empty unordered_map ---
    v_unordered_map(string n) : v_base(n, "unordered_map")
    {
        viz.update_state(v_name, v_type, data);
        viz.log_frame("Created empty unordered_map '" + v_name + "'.");
    }

    // --- Constructor 2: Create from a standard C++ unordered_map ---
    v_unordered_map(string n, const std::unordered_map<K, V> &initial_values) : v_base(n, "unordered_map"), data(initial_values)
    {
        viz.update_state(v_name, v_type, data);
        viz.log_frame("Created unordered_map '" + v_name + "' from initial data.");
    }

    // --- THE NEW FEATURE: Assignment from a std::unordered_map ---
    v_unordered_map<K, V> &operator=(const std::unordered_map<K, V> &new_values)
    {
        this->data = new_values;
        viz.update_state(v_name, v_type, this->data);
        viz.log_frame("Assigned new contents to unordered_map '" + v_name + "'.");
        return *this;
    }

    // The powerful proxy operator for reading and writing individual elements
    v_proxy<v_unordered_map<K, V>, K> operator[](K k) { return v_proxy<v_unordered_map<K, V>, K>(this, k); }
};
template <typename K, typename V>
class v_unordered_multimap : public v_base
{
public:
    using DataType = unordered_multimap<K, V>;
    unordered_multimap<K, V> data;
    // --- Constructor 1: Create an empty unordered_multimap ---
    v_unordered_multimap(string n) : v_base(n, "unordered_multimap")
    {
        viz.update_state(v_name, v_type, data);
        viz.log_frame("Created empty unordered_multimap '" + v_name + "'.");
    }

    // --- Constructor 2: Create from a standard C++ unordered_multimap ---
    v_unordered_multimap(string n, const std::unordered_multimap<K, V> &initial_values) : v_base(n, "unordered_multimap"), data(initial_values)
    {
        viz.update_state(v_name, v_type, data);
        viz.log_frame("Created unordered_multimap '" + v_name + "' from initial data.");
    }

    // --- THE NEW FEATURE: Assignment from a std::unordered_multimap ---
    v_unordered_multimap<K, V> &operator=(const std::unordered_multimap<K, V> &new_values)
    {
        this->data = new_values;
        viz.update_state(v_name, v_type, this->data);
        viz.log_frame("Assigned new contents to unordered_multimap '" + v_name + "'.");
        return *this;
    }

    // --- Visualizable Member Functions ---
    void insert(pair<K, V> p)
    {
        data.insert(p);
        viz.update_state(v_name, v_type, data, {{to_string(p.first), "write"}});
        viz.log_frame("Inserted pair (" + to_string(p.first) + ", " + to_string(p.second) + ") into '" + v_name + "'.");
    }
};
template <typename T>
class v_matrix : public v_base
{
public:
    using DataType = vector<vector<T>>;
    vector<vector<T>> data;
    v_matrix(string n, vector<vector<T>> iv) : v_base(n, "matrix"), data(iv)
    {
        viz.update_state(v_name, v_type, data);
        viz.log_frame("Created matrix '" + v_name + "' from initial data.");
    }
    v_proxy_2d_row<v_matrix> operator[](int r) { return v_proxy_2d_row<v_matrix>(this, r); }
};

// --- Helper for visualizing comparisons (FINAL, POLISHED VERSION) ---

// Base function that does the actual comparison and logging
int v_compare_base(long long val_a, long long val_b)
{
    viz.log_frame("Comparing " + to_string(val_a) + " and " + to_string(val_b));
    if (val_a < val_b)
        return -1;
    if (val_a > val_b)
        return 1;
    return 0;
}

// v_scalar vs v_scalar
template <typename T1, typename T2>
int v_compare(v_scalar<T1> &a, v_scalar<T2> &b)
{
    viz.update_state(a.v_name, a.v_type, a.data, {{"0", "compare"}});
    viz.update_state(b.v_name, b.v_type, b.data, {{"0", "compare"}});
    return v_compare_base(a.data, b.data);
}

// v_scalar vs normal value
template <typename T1, typename T2>
int v_compare(v_scalar<T1> &a, T2 b)
{
    viz.update_state(a.v_name, a.v_type, a.data, {{"0", "compare"}});
    return v_compare_base(a.data, b);
}

// normal value vs v_scalar
template <typename T1, typename T2>
int v_compare(T1 a, v_scalar<T2> &b)
{
    viz.update_state(b.v_name, b.v_type, b.data, {{"0", "compare"}});
    return v_compare_base(a, b.data);
}

// v_proxy (e.g., my_vec[i]) vs anything
template <typename P, typename K, typename T>
int v_compare(const v_proxy<P, K> &a, T b)
{
    // The read highlight is already done by the proxy's casting operator.
    // We just need to log the comparison message.
    return v_compare_base((int)a, b);
}
template <typename P, typename K, typename T>
int v_compare(const v_proxy<P, K> &a, v_scalar<T> &b)
{
    viz.update_state(b.v_name, b.v_type, b.data, {{"0", "compare"}});
    return v_compare_base((int)a, b.data);
}

// Helper to get value from a v_scalar (This is correct)
template <typename T>
T get(v_scalar<T> &s)
{
    return s;
}

// ########## UNIVERSAL INPUT PARSER & CONTEXT HANDLE ##########

// The InputParser is a self-contained class that turns a string like "arr={1,2}, k=5"
// into a C++ map that we can easily access.
class InputParser
{
private:
    string text;
    size_t pos = 0;

    void skip_whitespace()
    {
        while (pos < text.length() && isspace(text[pos]))
        {
            pos++;
        }
    }

    // Parses a variable name like 'arr', 'k', 'my_matrix'
    string parse_key()
    {
        skip_whitespace();
        size_t start = pos;
        if (pos < text.length() && isalpha(text[pos]))
        {
            pos++;
            while (pos < text.length() && (isalnum(text[pos]) || text[pos] == '_'))
            {
                pos++;
            }
        }
        return text.substr(start, pos - start);
    }

    // The main function that decides what kind of value to parse next
    json parse_value()
    {
        skip_whitespace();
        if (pos >= text.length())
            throw runtime_error("Unexpected end of input, expected a value.");

        char current = text[pos];
        if (current == '{')
        {
            pos++; // Consume '{'
            return parse_array_or_matrix();
        }
        if (current == '"')
        {
            pos++; // Consume '"'
            return parse_string();
        }
        if (isdigit(current) || current == '-')
        {
            return parse_number();
        }
        if (isalpha(current))
        {
            string literal = parse_key();
            if (literal == "true")
                return true;
            if (literal == "false")
                return false;
            throw runtime_error("Invalid value token: " + literal);
        }

        throw runtime_error("Invalid character in value: " + string(1, current));
    }

    // Parses a string value, e.g., "Hello World"
    json parse_string()
    {
        size_t start = pos;
        while (pos < text.length() && text[pos] != '"')
        {
            pos++;
        }
        if (pos >= text.length())
            throw runtime_error("Unterminated string literal.");
        string val = text.substr(start, pos - start);
        pos++; // Consume closing '"'
        return val;
    }

    // Parses a number, can be integer or floating point
    json parse_number()
    {
        size_t start = pos;
        if (text[pos] == '-')
            pos++;
        while (pos < text.length() && isdigit(text[pos]))
            pos++;
        if (pos < text.length() && text[pos] == '.')
        {
            pos++;
            while (pos < text.length() && isdigit(text[pos]))
                pos++;
            return stod(text.substr(start, pos - start));
        }
        return stoll(text.substr(start, pos - start)); // Use stoll for long long
    }

    // Parses an array like {1,2,3} or a matrix like {{1,2},{3,4}}
    json parse_array_or_matrix()
    {
        skip_whitespace();
        if (text[pos] == '}')
        { // Empty array {}
            pos++;
            return json::array();
        }

        json arr = json::array();
        while (pos < text.length())
        {
            arr.push_back(parse_value());
            skip_whitespace();
            if (text[pos] == '}')
            {
                pos++; // End of array
                return arr;
            }
            if (text[pos] == ',')
            {
                pos++; // Continue to next element
                continue;
            }
            throw runtime_error("Expected ',' or '}' in array declaration.");
        }
        throw runtime_error("Unterminated array declaration.");
    }

public:
    // The only public function. Takes the raw string and returns the parsed map.
    map<string, json> parse(const string &input)
    {
        text = input;
        pos = 0;
        map<string, json> result;

        while (pos < text.length())
        {
            string name = parse_key();
            if (name.empty())
            {
                skip_whitespace();
                if (pos < text.length())
                    throw runtime_error("Unexpected token at start of input.");
                break;
            }

            skip_whitespace();
            if (pos >= text.length() || text[pos] != '=')
            {
                throw runtime_error("Expected '=' after key '" + name + "'.");
            }
            pos++; // Consume '='

            result[name] = parse_value();
            skip_whitespace();
            if (pos < text.length() && text[pos] == ',')
            {
                pos++;
            }
        }
        return result;
    }
};

// The VCtx (Visualizer Context) easy-to-use handle for getting data.
class VCtx
{
private:
    map<string, json> p_input; // Holds the parsed data

    // Helper to log creation of new objects
    template <typename T, typename... Args>
    T create_and_log(Args &&...args)
    {
        T obj(std::forward<Args>(args)...);
        // The constructors of v_ objects already log, so we don't need to log here.
        return obj;
    }

public:
    VCtx(map<string, json> parsed_input) : p_input(std::move(parsed_input)) {}

    // --- Scalar Functions ---
    template <typename T>
    v_scalar<T> get_scalar(string name, T default_value = T{})
    {
        T value = p_input.count(name) ? p_input[name].get<T>() : default_value;
        return create_and_log<v_scalar<T>>(name, value);
    }
    template <typename T>
    v_scalar<T> new_scalar(string name, T initial_value = T{})
    {
        return create_and_log<v_scalar<T>>(name, initial_value);
    }

    // --- Vector Functions ---
    template <typename T>
    v_vector<T> get_vector(string name)
    {
        if (!p_input.count(name))
        {
            throw runtime_error("Input error: required vector '" + name + "' was not provided.");
        }
        return create_and_log<v_vector<T>>(name, p_input[name].get<vector<T>>());
    }
    template <typename T>
    v_vector<T> new_vector(string name, const std::vector<T> &iv = {})
    {
        return create_and_log<v_vector<T>>(name, iv);
    }

    // --- Matrix Functions ---
    template <typename T>
    v_matrix<T> get_matrix(string name)
    {
        if (!p_input.count(name))
        {
            throw runtime_error("Input error: required matrix '" + name + "' was not provided.");
        }
        return create_and_log<v_matrix<T>>(name, p_input[name].get<vector<vector<T>>>());
    }
    template <typename T>
    v_matrix<T> new_matrix(string name, const std::vector<vector<T>> &iv = {})
    {
        return create_and_log<v_matrix<T>>(name, iv);
    }

    // --- NEW: List Functions ---
    template <typename T>
    v_list<T> get_list(string name)
    {
        if (!p_input.count(name))
        {
            throw runtime_error("Input error: required list '" + name + "' was not provided.");
        }
        return create_and_log<v_list<T>>(name, p_input[name].get<list<T>>());
    }
    template <typename T>
    v_list<T> new_list(string name, const std::list<T> &iv = {})
    {
        return create_and_log<v_list<T>>(name, iv);
    }

    // --- NEW: Deque Functions ---
    template <typename T>
    v_deque<T> get_deque(string name)
    {
        if (!p_input.count(name))
        {
            throw runtime_error("Input error: required deque '" + name + "' was not provided.");
        }
        return create_and_log<v_deque<T>>(name, p_input[name].get<deque<T>>());
    }
    template <typename T>
    v_deque<T> new_deque(string name, const std::deque<T> &iv = {})
    {
        return create_and_log<v_deque<T>>(name, iv);
    }

    // --- NEW: Stack Functions ---
    // Note: Stacks are often created empty or from other containers, not directly from input.
    template <typename T>
    v_stack<T> new_stack(string name)
    {
        return create_and_log<v_stack<T>>(name);
    }
    template <typename T, typename Container>
    v_stack<T> new_stack(string name, const Container &iv)
    {
        return create_and_log<v_stack<T>>(name, iv);
    }

    // --- NEW: Queue Functions ---
    template <typename T>
    v_queue<T> new_queue(string name)
    {
        return create_and_log<v_queue<T>>(name);
    }
    template <typename T, typename Container>
    v_queue<T> new_queue(string name, const Container &iv)
    {
        return create_and_log<v_queue<T>>(name, iv);
    }

    // --- NEW: Priority Queue Functions ---
    template <typename T>
    v_priority_queue<T> new_priority_queue(string name)
    {
        return create_and_log<v_priority_queue<T>>(name);
    }
    template <typename T, typename Container>
    v_priority_queue<T> new_priority_queue(string name, const Container &iv)
    {
        return create_and_log<v_priority_queue<T>>(name, iv);
    }

    // --- NEW: Set Functions ---
    template <typename T>
    v_set<T> get_set(string name)
    {
        if (!p_input.count(name))
            throw runtime_error("Input error: required set '" + name + "' was not provided.");
        // We get a vector from JSON and construct the set from it
        auto vec = p_input[name].get<vector<T>>();
        return create_and_log<v_set<T>>(name, std::set<T>(vec.begin(), vec.end()));
    }
    template <typename T>
    v_set<T> new_set(string name, const std::set<T> &iv = {})
    {
        return create_and_log<v_set<T>>(name, iv);
    }

    // --- NEW: Multiset Functions ---
    template <typename T>
    v_multiset<T> get_multiset(string name)
    {
        if (!p_input.count(name))
            throw runtime_error("Input error: required multiset '" + name + "' was not provided.");
        auto vec = p_input[name].get<vector<T>>();
        return create_and_log<v_multiset<T>>(name, std::multiset<T>(vec.begin(), vec.end()));
    }
    template <typename T>
    v_multiset<T> new_multiset(string name, const std::multiset<T> &iv = {})
    {
        return create_and_log<v_multiset<T>>(name, iv);
    }

    // --- NEW: Map Functions ---
    // Note: Getting a map from JSON is tricky. The parser creates an array of key-value pairs.
    // We expect the input to be like: my_map={{key,val},{key,val}} which the parser handles as a vector of vectors.
    template <typename K, typename V>
    v_map<K, V> get_map(string name)
    {
        if (!p_input.count(name))
            throw runtime_error("Input error: required map '" + name + "' was not provided.");
        auto vec_of_pairs = p_input[name].get<vector<pair<K, V>>>(); // Requires a custom JSON->pair conversion
        return create_and_log<v_map<K, V>>(name, std::map<K, V>(vec_of_pairs.begin(), vec_of_pairs.end()));
    }
    template <typename K, typename V>
    v_map<K, V> new_map(string name, const std::map<K, V> &iv = {})
    {
        return create_and_log<v_map<K, V>>(name, iv);
    }

    // --- NEW: Multimap Functions ---
    template <typename K, typename V>
    v_multimap<K, V> get_multimap(string name)
    {
        if (!p_input.count(name))
            throw runtime_error("Input error: required multimap '" + name + "' was not provided.");
        auto vec_of_pairs = p_input[name].get<vector<pair<K, V>>>();
        return create_and_log<v_multimap<K, V>>(name, std::multimap<K, V>(vec_of_pairs.begin(), vec_of_pairs.end()));
    }
    template <typename K, typename V>
    v_multimap<K, V> new_multimap(string name, const std::multimap<K, V> &iv = {})
    {
        return create_and_log<v_multimap<K, V>>(name, iv);
    }

    // --- NEW: Unordered Set Functions ---
    template <typename T>
    v_unordered_set<T> get_unordered_set(string name)
    {
        if (!p_input.count(name))
            throw runtime_error("... '" + name + "' ...");
        auto vec = p_input[name].get<vector<T>>();
        return create_and_log<v_unordered_set<T>>(name, std::unordered_set<T>(vec.begin(), vec.end()));
    }
    template <typename T>
    v_unordered_set<T> new_unordered_set(string name, const std::unordered_set<T> &iv = {}) { return create_and_log<v_unordered_set<T>>(name, iv); }

    // --- NEW: Unordered Multiset Functions ---
    template <typename T>
    v_unordered_multiset<T> get_unordered_multiset(string name)
    {
        if (!p_input.count(name))
            throw runtime_error("... '" + name + "' ...");
        auto vec = p_input[name].get<vector<T>>();
        return create_and_log<v_unordered_multiset<T>>(name, std::unordered_multiset<T>(vec.begin(), vec.end()));
    }
    template <typename T>
    v_unordered_multiset<T> new_unordered_multiset(string name, const std::unordered_multiset<T> &iv = {}) { return create_and_log<v_unordered_multiset<T>>(name, iv); }

    // --- NEW: Unordered Map Functions ---
    template <typename K, typename V>
    v_unordered_map<K, V> get_unordered_map(string name)
    {
        if (!p_input.count(name))
            throw runtime_error("... '" + name + "' ...");
        auto v_p = p_input[name].get<vector<pair<K, V>>>();
        return create_and_log<v_unordered_map<K, V>>(name, std::unordered_map<K, V>(v_p.begin(), v_p.end()));
    }
    template <typename K, typename V>
    v_unordered_map<K, V> new_unordered_map(string name, const std::unordered_map<K, V> &iv = {}) { return create_and_log<v_unordered_map<K, V>>(name, iv); }

    // --- NEW: Unordered Multimap Functions ---
    template <typename K, typename V>
    v_unordered_multimap<K, V> get_unordered_multimap(string name)
    {
        if (!p_input.count(name))
            throw runtime_error("... '" + name + "' ...");
        auto v_p = p_input[name].get<vector<pair<K, V>>>();
        return create_and_log<v_unordered_multimap<K, V>>(name, std::unordered_multimap<K, V>(v_p.begin(), v_p.end()));
    }
    template <typename K, typename V>
    v_unordered_multimap<K, V> new_unordered_multimap(string name, const std::unordered_multimap<K, V> &iv = {}) { return create_and_log<v_unordered_multimap<K, V>>(name, iv); }
};

// ===============================================
// ==   THIS IS YOUR KINGDOM. YOUR PLAYGROUND.  ==
// ==   WRITE ALL YOUR LOGIC HERE,              ==
// ==   IN THIS run_my_algorithm FUNCTION       ==
// ===============================================

// --- User's Algorithm Function Declaration ---
// The user of the library MUST define this function.
void run_my_algorithm(VCtx& v);

// --- Your PLAYGROUND: Write your code here ---
std::string visualizeMyLogic(const std::string &raw_input)
{
    // ================================================================
    // BOILERPLATE START: This runs automatically before your code.
    // ================================================================

    viz.reset(); // Reset the engine for a new run

    try {
        // Setup Phase
        InputParser parser;
        map<string, json> parsed_input = parser.parse(raw_input);
        VCtx v(parsed_input); 

        viz.log_frame("Successfully parsed input.");

        // ================================================================
        // EXECUTION: Your personal algorithm function is called here.
        run_my_algorithm(v); 
        // ================================================================

    } catch (const std::exception& e) {
        viz.log_frame("Error: " + string(e.what()));
    }

    // ================================================================
    // BOILERPLATE END: This runs automatically after your code.
    // ================================================================
    return viz.history.dump(); // <-- STEP 2: The history is returned HERE.
}

// ##### EMSCRIPTEN BINDINGS #####
EMSCRIPTEN_BINDINGS(my_module)
{
    emscripten::function("visualizeMyLogic", &visualizeMyLogic);
}

#endif // V_CPP_HPP