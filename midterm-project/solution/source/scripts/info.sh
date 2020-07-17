
#!/bin/sh
# This is a comment!
grep -nr "# of students" test.txt | cut -d ' ' -f 1 > x1.txt 
grep -nr "# of students" test.txt | cut -d ' ' -f 16,17 > y1.txt
grep -nr "counter items" test.txt | cut -d ' ' -f 1 > x2.txt 
grep -nr "counter items" test.txt | cut -d ' ' -f 20 > y2.txt
grep -nr "The supplier delivered" test.txt | cut -d ' ' -f 1 > x3.txt 
grep -nr "The supplier delivered" test.txt | cut -d ' ' -f 10,11 > y3.txt 
 


