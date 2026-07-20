===============================================================================
                                                                             
                          S U P E R C O M P R E S S                          
                                                                             
                         Multithreaded .zarc Archiver                        
                             by msb -- 2026, HKMU                            
                                                                             
===============================================================================

PROGRAM:   SuperCompress
SYSTEM:    Qt / C++ (Cross-Platform GUI)
ENGINE:    zlib (via qCompress) + OpenMP

---[ DESCRIPTION ]-------------------------------------------------------------

SuperCompress is a robust, GUI-driven archiving utility optimized for bulk 
datasets. By combining Qt's native zlib compression with OpenMP parallel 
processing, it rapidly divides files into 16MB chunks and crushes them into 
our custom ".zarc" archive format. 

---[ FEATURES ]----------------------------------------------------------------

- MULTI-THREADED: OpenMP parallel processing for blazing fast chunking.
- BATCH LOADING: Recursively add entire directories with a single click.
- .ZARC FORMAT: Seamlessly packs and unpacks multiple files.
- RESPONSIVE GUI: Real-time progress tracking without UI freezing.

---[ USAGE ]-------------------------------------------------------------------

1. Click [ Add Files ] or [ Add Folder ] to build your queue.
2. Click [ Compress ] to generate your unified .zarc archive.
3. Click [ Decompress ] to extract an existing .zarc file.
4. Click [ Clear ] to reset your workspace.

---[ BUILD INSTRUCTIONS ]------------------------------------------------------

Requires Qt 5.x+, a C++ compiler, and OpenMP support. 
Add the following to your .pro file before compiling: QMAKE_CXXFLAGS += -fopenmp

[EOF]
