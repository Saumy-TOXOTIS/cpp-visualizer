// This is the only file the user needs to edit.

// Step 1: Include the framework library.
#include "v-cpp.hpp"
#include "libraries.hpp"

// Step 2: Write your algorithm inside this specific function.
void run_my_algorithm(VCtx& v)
{
    // ==========================================================
    // ==   ALGORITHM: Sliding Window Maximum (Corrected)      ==
    // ==========================================================

    auto arr = v.get_vector<int>("arr");
    auto k = v.get_scalar<int>("k");

    if ((int)k <= 0 || (int)k > arr.size()) {
        viz.log_frame("Error: Window size 'k' must be between 1 and the array size.");
        return;
    }

    auto dq = v.new_deque<int>("Candidate Indices (Deque)"); 
    auto result = v.new_vector<int>("Result (Max of each window)");
    auto i_ptr = v.new_scalar<int>("i"); // Using a different name to avoid shadowing loop variable

    viz.log_frame("Starting Sliding Window Maximum algorithm.");

    for (int i = 0; i < arr.size(); ++i)
    {
        i_ptr = i; // Update the visualizable pointer

        if (!dq.empty() && dq.data.front() <= i - (int)k) {
            viz.log_frame("Index " + to_string(dq.data.front()) + " is out of the window. Removing from front.");
            dq.pop_front();
        }

        // --- THE FIX: Be explicit to remove ambiguity ---
        // 1. Get the current value from the array and store it in a plain int.
        //    The read from arr[i] will be visualized here.
        int current_val = arr[i]; 

        // 2. Loop and compare `current_val` with the value at the back of the deque.
        while (!dq.empty()) {
            // Get the value from the back of the deque into a plain int.
            int back_val = arr[dq.data.back()];
            
            // Now compare the two plain integers. There is no ambiguity.
            if (back_val < current_val) {
                viz.log_frame("arr[" + to_string(i) + "]=" + to_string(current_val) + " is greater than arr[" + to_string(dq.data.back()) + "]=" + to_string(back_val) + ". Pruning back.");
                dq.pop_back();
            } else {
                // If the current value is not greater, stop pruning.
                break;
            }
        }

        viz.log_frame("Adding index " + to_string(i) + " to the back of the deque.");
        dq.push_back(i);

        if (i >= (int)k - 1) {
            int max_index = dq.data.front();
            int max_val = arr[max_index]; // Read into a variable for clarity
            viz.log_frame("Window complete. Max is arr[" + to_string(max_index) + "] = " + to_string(max_val));
            result.push_back(max_val);
        }
    }

    viz.log_frame("Algorithm finished. All window maximums have been found.");
}