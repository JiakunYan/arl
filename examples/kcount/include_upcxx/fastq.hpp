#ifndef _FASTQ_H
#define _FASTQ_H

#include <iostream>
#include <unistd.h>
#include <fcntl.h>
#include <zlib.h>
#include <upcxx/upcxx.hpp>

#include "utils.hpp"


using std::string;

using upcxx::rank_me;
using upcxx::rank_n;

#define INT_CEIL(numerator, denominator) (((numerator) - 1) / (denominator) + 1)
#define BUF_SIZE 2047

#define PER_RANK_FILE true

class FastqReader {

  FILE *f;
  gzFile gzf;
  off_t file_size;
  int64_t start_read;
  int64_t end_read;
  string fname;
  int max_read_len;
  char buf[BUF_SIZE + 1];

  void rtrim(string &s) {
    auto pos = s.length() - 1;
    while (pos >= 0 && std::isspace(static_cast<unsigned char>(s[pos]))) pos--;
    s.resize(pos + 1);
  }

  bool get_fq_name(string &header) {
    if (header[0] != '@') return false;
    // trim off '@'
    header.erase(0, 1);
    // convert if new illumina 2 format  or HudsonAlpha format
    int len = header.length();
    if (header[len - 2] != '/') {
      if (header[len - 2] == 'R') {
        // HudsonAlpha format  (@pair-R1,  @pair-R2)
        char *end = &header[len - 3];
        // replace with @pair/1 or @pair/2 */
        char rNum = header[len - 1];
        header[len - 3] = '/';
        header[len - 2] = rNum;
        header.resize(len - 1);
        return true;
      } else {
        // Latest Illumina header format
        auto end_pos = header.find('\t');
        if (end_pos != string::npos) {
          end_pos = header.find(' ');
          // no comment, just return without modification
          if (end_pos == string::npos) return true;
        }
        // check for @pair/1 @/pair2 vs @pair 1:Y:... @pair 2:Y:....
        if (end_pos > 3 && header[end_pos - 2] == '/' && (header[end_pos - 1] == '1' || header[end_pos - 1] == '2')) {
          // @pair/1 or @pair/2
          // truncate name at comment
          header.resize(end_pos);
          return true;
        }
        if ((len < end_pos + 7) || header[end_pos + 2] != ':' || header[end_pos + 4] != ':' || header[end_pos + 6] != ':' ||
            (header[end_pos + 1] != '1' && header[end_pos + 1] != '2')) {
          // unknown pairing format
          return false;
        }
        // @pair 1:Y:.:.: or @pair 2:Y:.:.:
        // replace with @pair/1 or @pair/2
        header[end_pos] = '/';
        header.resize(end_pos + 2);
      }
    }
    return true;
  }

  int64_t get_fptr_for_next_record(int64_t offset) {
    // first record is the first record, include it.  Every other partition will be at least 1 full record after offset.
    if (offset == 0) return 0;
    if (offset >= file_size) return file_size;
    if (fseek(f, offset, SEEK_SET) != 0) DIE("Could not fseek in ", fname, " to ", offset, ": ", strerror(errno));
    // skip first (likely partial) line after this offset to ensure we start at the beginning of a line
    if (!fgets(buf, BUF_SIZE, f)) return ftell(f);

    for (int i = 0; ; i++) {
      if (!fgets(buf, BUF_SIZE, f)) return ftell(f);
      // keep reading new lines until we find a valid header ending in 2 for the second of the pair
      if (buf[0] == '@') {
        string header(buf);
        rtrim(header);
        if (!get_fq_name(header)) continue;
        // need this to be the second of the pair
        if (header[header.length() - 1] == '2') {
          // now read another three lines and be done
          for (int j = 0; j < 3; j++) {
            if (!fgets(buf, BUF_SIZE, f)) DIE("Missing record info at pos ", ftell(f));
          }
          break;
        }
      }
      if (i > 13) DIE("Could not find a valid line in the fastq file ", fname, ", last line: ", buf);
    }
    return ftell(f);
  }

public:

  FastqReader(const string &fname, bool per_rank_file=false)
    : fname(fname)
    , f(nullptr)
    , gzf(nullptr)
    , max_read_len(0) {

    if (!per_rank_file) {
      // only one rank gets the file size, to prevent many hits on metadata
      if (!rank_me()) file_size = get_file_size(fname);
      file_size = upcxx::broadcast(file_size, 0).wait();
    }
    f = fopen(fname.c_str(), "r");
    if (!f) {
      if (!upcxx::rank_me()) DIE("Could not open file ", fname, ": ", strerror(errno));
      upcxx::barrier();
    }
    // just a part of the file is read by this thread
    int max_rank = (per_rank_file ? 1 : rank_n());
    int64_t read_block = INT_CEIL(file_size, max_rank);
    int my_rank = (per_rank_file ? 0 : rank_me());
    start_read = read_block * my_rank;
    end_read = read_block * (my_rank + 1);
    if (my_rank) start_read = get_fptr_for_next_record(start_read);
    if (my_rank == max_rank - 1) end_read = file_size;
    else end_read = get_fptr_for_next_record(end_read);
    if (fseek(f, start_read, SEEK_SET) != 0) DIE("Could not fseek on ", fname, " to ", start_read, ": ", strerror(errno));
    posix_fadvise(fileno(f), start_read, end_read - start_read, POSIX_FADV_SEQUENTIAL);
    SLOG_VERBOSE("Reading FASTQ file ", fname, "\n");
    DBG("Reading fastq file ", fname, " at pos ", start_read, " ", (f ? ftell(f) : gztell(gzf)), "\n");
  }

  ~FastqReader() {
    if (f) fclose(f);
    if (gzf) gzclose(gzf);
  }

  size_t my_file_size() {
    return end_read - start_read;
  }

  long tell() {
    return (f ? ftell(f) : gztell(gzf));
  }

  size_t get_next_fq_record(string &id, string &seq, string &quals) {
    if (f) {
      if (feof(f) || ftell(f) >= end_read) return 0;
    } else {
      if (gzeof(gzf) || gztell(gzf) >= end_read) return 0;
    }
    size_t bytes_read = 0;
    for (int i = 0; i < 4; i++) {
      char *bytes = (f ? fgets(buf, BUF_SIZE, f) : gzgets(gzf, buf, BUF_SIZE));
      if (!bytes) DIE("Read record terminated on file ", fname, " before full record at position ", (f ? ftell(f) : gztell(gzf)));
      if (i == 0) id.assign(buf);
      else if (i == 1) seq.assign(buf);
      else if (i == 3) quals.assign(buf);
      bytes_read += strlen(buf);
    }
    rtrim(id);
    rtrim(seq);
    rtrim(quals);
    if (id[0] != '@')
      DIE("Invalid FASTQ in ", fname, ": expected read name (@), got: ", id);
    // construct universally formatted name (illumina 1 format)
    if (!get_fq_name(id))
      DIE("Invalid FASTQ in ", fname, ": incorrect name format '", id, "'");
    // get rid of spaces
    // FIXME: this should be hexify
    replace_spaces(id);
    if (seq.length() != quals.length())
      DIE("Invalid FASTQ in ", fname, ": sequence length ", seq.length(), " != ", quals.length(), " quals length\n");
    if (seq.length() > max_read_len) max_read_len = seq.length();
    return bytes_read;
  }

  size_t get_next_fq_record(uint64_t &id, uint8_t pair_idx, string &seq, string &quals) {
    string id_str;
    auto res = get_next_fq_record(id_str, seq, quals);
    if (id_str[0] != 'r') DIE("Expected compressed read id beginning with r and then a number. Found ", id_str);
    id = std::stoull(id_str.substr(1, id_str.length() - 3));
    pair_idx = id_str[id_str.length() - 1] == '1' ? 1 : 2;
    return res;
  }

};

#endif
