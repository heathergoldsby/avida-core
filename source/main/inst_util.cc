//////////////////////////////////////////////////////////////////////////////
// Copyright (C) 1993 - 2003 California Institute of Technology             //
//                                                                          //
// Read the COPYING and README files, or contact 'avida@alife.org',         //
// before continuing.  SOME RESTRICTIONS MAY APPLY TO USE OF THIS FILE.     //
//////////////////////////////////////////////////////////////////////////////

#include "inst_util.hh"

#include "../tools/string.hh"
#include "../tools/file.hh"

#include "../cpu/hardware_method.hh"
#include "inst_set.hh"
#include "genome.hh"


using namespace std;


cGenome cInstUtil::LoadGenome(const cString & filename,
			      const cInstSet & inst_set)
{
    cInitFile input_file(filename);
  if (!input_file.IsOpen()) {
    cerr << "Cannot open file: " << filename << endl;
    return cGenome(1);
  }
  input_file.Load();
  input_file.Compress();
  input_file.Close();

  // Setup the code array...
  cGenome new_genome(input_file.GetNumLines());

  for (int line_num = 0; line_num < new_genome.GetSize(); line_num++) {
    cString cur_line = input_file.GetLine(line_num);
    new_genome[line_num] = inst_set.GetInst(cur_line);

    if (new_genome[line_num] == cInstSet::GetInstError()) {
      // You're using the wrong instruction set!  YOU FOOL!
      cerr << "Cannot load organism '" << filename << "'" << endl
	   << "       Unknown line: " << cur_line
	   << " (best match is '" << inst_set.FindBestMatch(cur_line) << "')"
	   << endl;
      exit(1);
    }
  }

  if(new_genome.GetSize()==0)
    cerr << "WARNING: Genome size is 0!" << endl;

  return new_genome;
}

cGenome cInstUtil::LoadInternalGenome(istream & fp, const cInstSet &inst_set)
{
  assert(fp.good()); // Invalid stream to load genome from!

  int num_lines = -1;
  fp >> num_lines;

  if (num_lines <= 0) { return cGenome(1); }

  // Setup the code array...
  cGenome new_genome(num_lines);
  cString cur_line;

  for (int line_num = 0; line_num < new_genome.GetSize(); line_num++) {
    fp >> cur_line;
    new_genome[line_num] = inst_set.GetInst(cur_line);

    if (new_genome[line_num] == cInstSet::GetInstError()) {
      // You're using the wrong instruction set!  YOU FOOL!
      cerr << "Cannot load creature from stream:" << endl
	   << "       Unknown line: " << cur_line << endl;
    }
  }
  return new_genome;
}

void cInstUtil::SaveGenome(ostream & fp, const cInstSet & inst_set,
			   const cGenome & gen)
{
  for (int i = 0; i < gen.GetSize(); i++) {
    fp << inst_set.GetName(gen[i]) << endl;
  }
}

void cInstUtil::SaveInternalGenome(ostream & fp, const cInstSet & inst_set,
				   const cGenome & gen)
{
  fp << gen.GetSize() << endl;
  SaveGenome(fp, inst_set, gen);
}


cGenome cInstUtil::RandomGenome(int length, const cInstSet & inst_set)
{
  cGenome genome(length);
  for (int i = 0; i < length; i++) {
    genome[i] = inst_set.GetRandomInst();
  }
  return genome;
}

