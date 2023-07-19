
## A grammar self-index based on LMS substrings

A grammar self-index of a text $T$ (Claude et al. 2012) consists of a grammar $\mathcal{G}$ that only produces $T$ and a geometric data structure that indexes the string cuts of the right-hand sides of $\mathcal{G}$'s rules. This representation uses space proportional to $G$, the size of the grammar, which is small when the text is repetitive. However, the index is slow for pattern matching; it finds the $occ$ occurrences of a pattern $P[1..m]$ in $O((m^{2}+occ)\log G)$ time. The most expensive part is a set of binary searches for the different cuts $P[1..j]P[j+1..m]$ in the geometric data structure. Christiansen et al. (2019) solved this problem by building a locally consistent grammar that only searches for $O(\log m)$ cuts of $P$. Their representation, however, requires significant extra space (tough still in $O(G)$) to store a set of permutations for the nonterminal symbols. In this work, we propose another locally consistent grammar that builds on the idea of LMS substrings (Nong et al. 2009). Our grammar also requires to try $O(\log m)$ cuts when searching for $P$, but it does not need to store permutations. 
As a result, we obtain a self-index that searches in time $O((m\log m+occ) \log G)$ and is of practical size. Our experiments showed that our index is faster than previous grammar-indexes at the price of increasing the space by a 1.8x factor on average. Other experimental results showed that our data structure becomes convenient when the patterns to search for are long.

## Third-party libraries

1. [SDSL-lite](https://github.com/simongog/sdsl-lite)
2. [xxHash](https://github.com/Cyan4973/xxHash)

## Prerequisites

1. C++ >= 17
2. CMake >= 3.7
3. SDSL-lite

The xxHash library is already included in the source files. We include a CMake module that will search for
the local installation of the SDSL-library. No need to indicate the path during the compilation.

## Installation

Clone repository, enter the project folder and execute
the following commands:

```
mkdir build
cd build
cmake ..
make
```

## Creating the index
```
./lpg index tests/sample_file.txt
```

The command above will produce the file **sample_file.txt.lpg_idx** 

### Input for the index 

The current implementation expects a string ending with the null '\0' character. If you have a collection rather than a
single string, you need to concatenate the input into one sequence and then append '\0'. Notice that the program will crash if '\0' appears in other places
of the file different from the end.

## Search for a pattern

Assuming you are in the folder ``build`` inside the repository. You can run a search example as: 
```
./lpg search sample_file.txt.lpg_idx -F ../tests/sample_file.rand_pat_100_10
```

The ``-F`` flag expects a file with the pattern list (one element per line). Alternatively, you can use ``--p`` to pass a pattern
in place. The in-place option can take multiple inputs. For instance, ``--p pat1 pat2 pat3 ..`` or
``--p pat1 --p pat2 --p pat3``. The options ``-F`` and ``--p`` are complementary, meaning that the program will search for
the combined pattern collection. 

## Result of the search

The ``-r`` flag in the command line will report the number of occurrences and the elapsed time individually per input
pattern. If you do not use this flag, the program will print the sum of all the pattern occurrences and the total
elapsed time to get them.

## Disclaimer 

This repository is a legacy implementation that has yet to be tested in massive inputs.
If you find bugs, please report them here.

## Citation

If you use this code, please cite the following paper:

```
Díaz-Domínguez, D., Navarro, G., & Pacheco, A..
An LMS-based grammar self-index with local consistency properties.
In Proc. 28th Symposium on String Processing and Information (SPIRE 2021). 
```

