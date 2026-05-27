import random
import math

# 1. Cube Geometry (Same as before)
turns = {
    'U': ([0, 1, 2, 3], False),
    'D': ([4, 7, 6, 5], False),
    'F': ([1, 9, 5, 8], True),
    'B': ([3, 11, 7, 10], True),
    'L': ([2, 10, 6, 9], False),
    'R': ([0, 8, 4, 11], False)
}

transitions = []
for move, (cycle, flips) in turns.items():
    for i in range(4):
        src_slot, dst_slot = cycle[i], cycle[(i + 1) % 4]
        for ori in (0, 1):
            src_state = src_slot * 2 + ori
            dst_state = dst_slot * 2 + (1 - ori if flips else ori)
            transitions.append((src_state, dst_state))

def hamming(a, b):
    return bin(a ^ b).count('1')

def calculate_gray_cost(mapping):
    """
    Cost function designed to find a Gray-code-like embedding.
    We want distance to be exactly 1.
    """
    total_cost = 0
    for src, dst in transitions:
        dist = hamming(mapping[src], mapping[dst])
        if dist == 1:
            total_cost += 0 # Perfect Gray transition
        else:
            # High penalty for non-Gray transitions (2, 3, 4, or 5 bits)
            # Using 10^dist makes a 2-bit change 10x worse than a 1-bit change
            total_cost += 10 ** dist 
    return total_cost

def optimize_gray_mapping():
    pool = list(range(32))
    random.shuffle(pool)
    mapping = pool[:24]
    
    current_cost = calculate_gray_cost(mapping)
    
    # High-Performance Parameters
    temp = 50.0 # Start hotter to explore more of the hypercube
    cooling_rate = 0.999995 # Very slow cooling
    iterations = 1000000 # 1 million iterations
    
    best_mapping = list(mapping)
    best_cost = current_cost
    
    for i in range(iterations):
        new_mapping = list(mapping)
        
        # Strategy: Occasionally swap with unused bits to find better 'paths'
        if random.random() < 0.5:
            idx1, idx2 = random.sample(range(24), 2)
            new_mapping[idx1], new_mapping[idx2] = new_mapping[idx2], new_mapping[idx1]
        else:
            idx = random.randrange(24)
            unused = list(set(range(32)) - set(new_mapping))
            new_mapping[idx] = random.choice(unused)
            
        new_cost = calculate_gray_cost(new_mapping)
        
        if new_cost < current_cost or random.random() < math.exp((current_cost - new_cost) / temp):
            mapping = new_mapping
            current_cost = new_cost
            if new_cost < best_cost:
                best_cost = new_cost
                best_mapping = list(mapping)
                
        temp *= cooling_rate
        if i % 100000 == 0:
            print(f"Iteration {i}: Current Best Cost = {best_cost}")
            
    return best_mapping

print("Searching for Gray-Code Embedding (this will take a minute)...")
best_map = optimize_gray_mapping()

# Results Analysis
dists = [hamming(best_map[s], best_map[d]) for s, d in transitions]
print(f"\nResults:")
print(f"- Transitions that are perfect Gray-code (1 bit): {dists.count(1)}")
print(f"- Transitions that are 2-bit flips: {dists.count(2)}")
print(f"- Transitions that are 3+ bit flips: {sum(1 for d in dists if d > 2)}")
print(f"- Average bits per piece move: {sum(dists)/len(dists):.2f}")
