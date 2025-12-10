python3 -m venv .venv
source .venv/bin/activate
pip install numpy

# cat ../out/out_heuristic_improve_m* | grep "moves:" | sed 's/.*moves: //' | sed -r 's/\x1b\[[0-9;]*m//g' > heuristic_improve.txt
