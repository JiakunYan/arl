cat test*.dat | sort > my_solution.txt
diff my_solution.txt datasets/$1_solution.txt
