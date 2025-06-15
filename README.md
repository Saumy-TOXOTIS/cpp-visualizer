# v-cpp: A Modern C++ Algorithm Visualization Framework

<p align="center">
  <img src="https://img.shields.io/badge/Language-C++20-blue.svg" alt="Language">
  <img src="https://img.shields.io/badge/Frontend-React-61DAFB.svg" alt="Frontend">
  <img src="https://img.shields.io/badge/Compiler-Emscripten_(Wasm)-f0db4f.svg" alt="Compiler">
  <img src="https://img.shields.io/badge/License-MIT-green.svg" alt="License">
</p>

**v-cpp** is an advanced, header-only C++ library that brings algorithms to life by compiling C++ code to high-performance **WebAssembly** and rendering its step-by-step execution in a beautiful, interactive **React** frontend. This project bridges the gap between high-performance systems programming and modern web technology, allowing you to write complex C++ logic and see it visualized instantly in your browser.

## ✨ Features

*   **Seamless C++ to Web:** Write standard C++ code in a dedicated "playground" function. The Emscripten toolchain compiles it to WebAssembly, making it executable on the web at near-native speed.
*   **Automated Visualization Engine:** A sophisticated C++ framework with wrapper classes for all major STL containers (`vector`, `map`, `set`, `stack`, `list`, `queue`, etc.). State changes to these objects are automatically logged to create a detailed, step-by-step animation of your algorithm's execution.
*   **Powerful Universal Input Parser:** A robust, key-value based parser allows you to provide complex, structured input in any order. It supports scalars, nested arrays (for matrices), and pairs (for maps), making test cases flexible and easy to write.
*   **Fluent C++ API:** A clean and intuitive context handle (`VCtx`) provides a simple interface for creating visualizable data structures from parsed input or from scratch, keeping your algorithm code clean and focused.
*   **Modern React UI:** A responsive, visually appealing frontend built with React, featuring full playback controls (play, pause, step forward/backward, slider) to inspect every step of the algorithm.

## 🚀 Why v-cpp?

Traditional C++ algorithm development often involves debugging with `printf` statements or complex desktop-based debuggers. v-cpp was built to provide a more intuitive and insightful way to understand how data structures change over time.

*   **For Students & Educators:** An excellent tool for learning and teaching complex algorithms like sorting, graph traversals, or dynamic programming.
*   **For Competitive Programmers:** Quickly debug and verify the logic of an algorithm by seeing exactly how the data is being manipulated at each step.
*   **For Developers:** A powerful demonstration of modern C++ (C++20), software architecture, and full-stack development principles, integrating low-level code with high-level web interfaces.

## 🛠️ How It Works: The Architecture

The project is architected with a clean separation of concerns:

1.  **C++ Framework (`v-cpp.hpp`):** A header-only C++ library containing the core visualization engine (`VizEngine`), wrapper classes for data structures (`v_vector`, `v_scalar`, etc.), and the input parser. This is the heart of the project.
2.  **User Algorithm (`my_algorithm.cpp`):** The only file the user needs to edit. You include the framework and write your logic inside a single `run_my_algorithm(VCtx& v)` function.
3.  **Emscripten/Wasm Bridge:** Emscripten compiles the C++ code into a `.wasm` binary and a `.js` loader file. The `visualizeMyLogic` function is exported and made callable from JavaScript.
4.  **React Frontend (`App.js`):** The user interacts with a React application. When "Visualize!" is clicked, the input string is passed to the Wasm module. The Wasm module executes the algorithm and returns a complete history of all steps as a single JSON string.
5.  **Rendering:** React parses the JSON history and uses it to render the state of all data structures for each frame, allowing the user to play back the entire execution.

## 💻 Getting Started

### Prerequisites

*   [Node.js](https://nodejs.org/) and npm
*   [Emscripten SDK](https://emscripten.org/docs/getting_started/downloads.html) (activated in your terminal)

### Installation & Running

1.  **Clone the repository:**
    ```bash
    git clone https://github.com/[YOUR-USERNAME]/cpp-visualizer.git
    cd cpp-visualizer
    ```

2.  **Install frontend dependencies:**
    ```bash
    npm install
    ```

3.  **Compile the C++ to WebAssembly:**
    Run the compilation script. This command takes your C++ algorithm, compiles it, and places the output (`algorithms.js` and `algorithms.wasm`) in the `public/wasm` directory.
    ```bash
    emcc -std=c++20 src/cpp/my_algorithm.cpp -o public/wasm/algorithms.js -O3 -s WASM=1 -s MODULARIZE=1 -s "EXPORT_NAME='createAlgoModule'" -s EXPORTED_RUNTIME_METHODS='["cwrap"]' --bind -I src/cpp/
    ```

4.  **Start the React development server:**
    ```bash
    npm start
    ```
    Your browser should open to `http://localhost:3000`, where you can now use the visualizer.

## ✍️ Writing Your Own Algorithm

Modifying the visualizer is incredibly simple:

1.  **Open `src/cpp/my_algorithm.cpp`**. This is your dedicated playground.
2.  **Write your logic** inside the `void run_my_algorithm(VCtx& v)` function.
3.  Use the `v` handle to get data from the user input (e.g., `auto arr = v.get_vector<int>("arr");`) or create new visualizable objects (e.g., `auto my_stack = v.new_stack<int>("My Stack");`).
4.  Re-run the `emcc` compilation command (Step 3 above) to see your new algorithm in action.

### Universal Input Guide

The input parser uses a simple `key=value` format.

| Data Type      | Example Input                                     | C++ `VCtx` Usage                           |
| :------------- | :------------------------------------------------ | :----------------------------------------- |
| **Scalar**     | `count=100, name="Saumy", active=true`            | `v.get_scalar<int>("count")`               |
| **Vector/List**| `arr={10, 20, 30}`                                | `v.get_vector<int>("arr")`                 |
| **Matrix**     | `matrix={{1, 2}, {3, 4}}`                         | `v.get_matrix<int>("matrix")`              |
| **Map**        | `scores={ {"p1", 100}, {"p2", 95} }`                | `v.get_map<string, int>("scores")`         |

Keys must match the string used in your C++ `get_...` call. Pairs are separated by commas.
