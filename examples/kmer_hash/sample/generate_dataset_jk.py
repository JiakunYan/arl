import random
import time

k = 51

contig_length = 100
num_to_gen = 1024

alphabet = ['A', 'C', 'T', 'G']
spacer = ' '

kmers = set()

def get():
    return random.choice(alphabet)

def gen_kmer():
    return ''.join([get() for x in range(k)])

def gen_unique_kmer():
    kmer = gen_kmer()
    while kmer in kmers:
        kmer = gen_kmer()
    kmers.add(kmer)
    return kmer

def gen_contig():
    start_kmer = gen_unique_kmer()

    kmer_list = [start_kmer]

    while (len(kmer_list) < contig_length):
        i = 0
        random_base = get()
        hey = kmer_list[-1]
        next_kmer = kmer_list[-1][1:] + random_base
        while next_kmer in kmers and i < 10:
            random_base = get()
            next_kmer = kmer_list[-1][1:] + random_base
            i += 1
        if next_kmer in kmers:
            return kmer_list
        else:
            kmers.add(next_kmer)
            kmer_list.append(next_kmer)
    return kmer_list

def extend_contig(contig):
    extended_contig = [contig[0] + spacer + 'F' + contig[1][-1]]

    for i in range(1, len(contig)-1):
        extended_contig.append(contig[i] + spacer + contig[i-1][0] + contig[i+1][-1])

    extended_contig.append(contig[-1] + spacer + contig[-2][0] + 'F')
    return extended_contig



#for i in range(num_to_gen):
#    kmer = gen_kmer()
#    while kmer in kmers:
#        kmer = gen_kmer()
#    kmers.add(kmer)

lengths = []

f = open('kmers.dat', 'w')
start_time = time.time()
for i in range(num_to_gen):
    if i > 0 and i % 100 == 0:
        elapsed_time = time.time() - start_time
        rate = elapsed_time / i
        time_left = rate * (num_to_gen - i)
        time_left /= 60
        print('%s / %s (avg length: %s, time left: %s minutes)' % (i, num_to_gen, sum(lengths) / len(lengths), time_left))
    contig = gen_contig()
    lengths.append(len(contig))
    contig = extend_contig(contig)

    for kmer in contig:
        f.write('%s\n' % (kmer,))

print('%s' % (sum(lengths) / len(lengths)))

f.close()
