#ifndef __LUNAIX_FSTYPES_H
#define __LUNAIX_FSTYPES_H

/*
  7 6 5 4 3   2 1 0
  * * s P SV  D d f
      | | |   | | |_ file
      | | |   | |___ directory
      | | |   |_____ Device
      | | |_________ Seq/Vol (0: Seq; 1: Vol)
      | |___________ Pipe
      |_____________ symlink 
  
*/

#define F_FILE      0b00000001
#define F_DIR       0b00000010
#define F_DEV       0b00000100
#define F_SVDEV     0b00001000
#define F_PIPE      0b00010000
#define F_SYMLINK   0b00100000

#endif /* __LUNAIX_FSTYPES_H */
