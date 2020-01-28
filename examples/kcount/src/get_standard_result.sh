#!/bin/bash
find per_rank/ -name "*.kmers.gz" | xargs gzip -k -d
find per_rank/ -name "*.kmers" | xargs cat | cat > all.kmers
sort all.kmers > all.kmers.sort
find per_rank/ -name "*.kmers" | xargs wc -l
wc -l all.kmers.sort