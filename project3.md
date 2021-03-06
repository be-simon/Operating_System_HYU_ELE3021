# Milestone 1

## ๐ฏ Implement sync system call

### log.c

`int sync (void)`

- ๋ฒํผ ๋ฐ์ดํฐ๋ค์ ๋ณ๊ฒฝ์ฌํญ์ log์ ์จ์ฃผ๊ณ  disk์ ๋ฐ์ํ๋ค (commit)
- ๋ง์ฝ ์ปค๋ฐ ์ค์ด๊ฑฐ๋ log space๊ฐ ์ฌ์ฉ ์ค์ด๋ผ๋ฉด sleep ์ํ๋ก ๊ธฐ๋ค๋ฆฐ๋ค.

```c
int
sync(void)
{
	acquire(&log.lock);
	
	while(log.committing || log.outstanding)	
		sleep(&log, &log.lock);

	log.committing = 1;
	release(&log.lock);

	commit();
	acquire(&log.lock);
	log.committing = 0;
	wakeup(&log);
	release(&log.lock);

	return 0;
}
```

`int get_log_num (void)`

- ๋ก๊ทธ ํค๋์ ๋ณ๊ฒฝ๋ ๋ธ๋ญ์ ์๋ฅผ ๋ฆฌํดํ๋ค.

```c
int 
get_log_num(void)
{
	return log.lh.n; 
}
```

`void end_op (void)`

- write๋ฅผ ํ  ๋ ๋ฐ๋ก ์ปค๋ฐํ์ง ์๊ณ  ๋ฒํผ์๋ง ๊ธฐ๋กํด๋ ํ, sync ์์คํ ์ฝ ํธ์ถ ๋๋ log space๊ฐ ๊ฐ๋ ์ฐผ์ ๋ ๋์คํฌ์ ์จ์ผํ๋ฏ๋ก ๋ก๊ทธ๊ฐ ์ฐผ์ ๋๋ง ์ปค๋ฐ์ ํ๋๋ก ๋ณ๊ฒฝํด์ค๋ค.

```c
void
end_op(void)
{
	  ...
  if(log.outstanding == 0 && log.lh.n + MAXOPBLOCKS > LOGSIZE){
    do_commit = 1;
    log.committing = 1;
  } else {
		...
  }
}
```

# Milestone 2

## ๐ฏ Expand the maximum size of a file (Triple indirect block ๊ตฌํ)

### fs.h

- Triple indirect๋ก ๋ณ๊ฒฝํ๋ฉด์ `NDIRECT`์ ์๋ ์ค์ด์ผํ๋ค.
- ๋์  `NINDERECT` ์ ์ ๊ณฑ์ธ  `NDINDIRECT` ์ ์ธ์ ๊ณฑ์ธ `NTINDIRECT`๋ฅผ ์๋ก ์ ์ธํ๋ค.
- `MAXFILE` ์ ๊ฐ์ ์ถ๊ฐ๋ indirect block๋ค ๋งํผ ๋ํด์ค๋ค.

```c
#define NDIRECT 10
#define NINDIRECT (BSIZE / sizeof(uint))
#define NDINDIRECT (NINDIRECT * NINDIRECT)
#define NTINDIRECT (NDINDIRECT * NINDIRECT)
#define MAXFILE (NDIRECT + NINDIRECT +NDINDIRECT + NTINDIRECT)
```

- `dinode` ๊ตฌ์กฐ์ฒด์ `addrs`๋ ๋ณ๊ฒฝํด์ค๋ค.

```c
struct dinode {
	...
	uint addrs[NDIRECT+3];   // Data block addresses
};
```

### file.h

- `dinode`์ ๋ง์ฐฌ๊ฐ์ง๋ก `inode` ๊ตฌ์กฐ์ฒด์์๋ `addrs`์ ์์ ํด์ค๋ค.

```c
struct inode {
  ...
  uint addrs[NDIRECT+3];
};
```

### fs.c

`static uint bmap (struct inode *ip, uint bn)`

- ๊ธฐ์กด์ single indirect block์ ์ฝ๋ ๊ฒ์ ๋ํด์, ๊ทธ ์ด์์ ๋ธ๋ญ ๋๋ฒ์ ๋ํด 2์ฐจ ๋ฐ 3์ฐจ ๋ธ๋ญ์ ์ฝ์ด์ค๋๋ก ์์ ํ๋ค.

```c
static uint
bmap(struct inode *ip, uint bn)
{
  uint addr, *a, dbn, tbn;
  struct buf *bp;

  if(bn < NDIRECT){
    ...
  }
  bn -= NDIRECT;

  if(bn < NINDIRECT){
    // single indirct 
    ...
  }
	
	bn -= NINDIRECT;
	// doubly indirect
	if(bn < NDINDIRECT) {
		if ((addr = ip->addrs[NDIRECT + 1]) == 0)
			ip->addrs[NDIRECT + 1] = addr = balloc(ip->dev);
		bp = bread(ip->dev, addr);
		a = (uint*)bp->data;

		dbn = bn / NINDIRECT;

		if ((addr = a[dbn]) == 0) {
			a[dbn] = addr = balloc(ip->dev);
			log_write(bp);
		}
		brelse(bp);

		bn %= NINDIRECT;
		bp = bread(ip->dev, addr);
		a = (uint*)bp->data;
		if ((addr = a[bn]) == 0) {
			a[bn] = addr = balloc(ip->dev);
			log_write(bp);
		}
		brelse(bp);
		return addr;
	}
	bn -= NDINDIRECT;

	if (bn < NTINDIRECT) {
		// triple indirect
		if ((addr = ip->addrs[NDIRECT + 2]) == 0)
			ip->addrs[NDIRECT + 2] = addr = balloc(ip->dev);
		bp = bread(ip->dev, addr);
		a = (uint*)bp->data;

		dbn = bn / NDINDIRECT;

		if ((addr = a[dbn]) == 0) {
			a[dbn] = addr = balloc(ip->dev);
			log_write(bp);
		}
		brelse(bp);
	
		bn %= NDINDIRECT;
		tbn = bn / NINDIRECT;

		bp = bread(ip->dev, addr);
		a = (uint*)bp->data;
		if ((addr = a[tbn]) == 0) {
			a[tbn] = addr = balloc(ip->dev);
			log_write(bp);
		}
		brelse(bp);

		bn %= NINDIRECT;
		bp = bread(ip->dev, addr);
		a = (uint*)bp->data;
		if((addr = a[bn]) == 0) {
			a[bn] = addr = balloc(ip->dev);
			log_write(bp);
		}
		brelse(bp);
		return addr;
	}

  panic("bmap: out of range");
}
```

- `dbn` ๊ณผ `tbn` ์ ์ ์ธํด์ 2์ฐจ, 3์ฐจ ๋ธ๋ญ์์ ๋ช๋ฒ์งธ ์ํธ๋ฆฌ์ ์ ๊ทผํด์ผํ๋์ง ์ ์ฅํด๋๋ค.

`static void itrunc (struct inode *ip)`

- 2์ฐจ, 3์ฐจ๊น์ง ๋ธ๋ญ์ด ํ์ฑ๋์ด์์ ์ ์์ผ๋ฏ๋ก, 2์ฐจ ๋ฐ 3์ฐจ ๋ธ๋ญ๋ค๋ ํ์ธ ํ ๋ธ๋ญ์ ํด์ ํด์ค๋ค.
- ๊ฐ iteration์์ ํ ๋น๋ ๋ธ๋ญ์ด ์๋์ง ํ์ธํ๊ณ  ์๋ค๋ฉด free ํด์ค๋ค.

```c
static void
itrunc(struct inode *ip)
{
  int i, j, k;
  struct buf *bp, *dbp, *tbp;
  uint *a, *da, *ta;

  for(i = 0; i < NDIRECT; i++){
   //direct
		...
  }

  if(ip->addrs[NDIRECT]){
   // single indirect
		...
  }

	if (ip->addrs[NDIRECT+1]) {
		bp = bread(ip->dev, ip->addrs[NDIRECT+1]);
		a = (uint*)bp->data;

		for (i = 0; i < NINDIRECT; i++) {
			if(a[i]) {
				dbp = bread(ip->dev, a[i]);
				da = (uint*)dbp->data;
				for (j = 0; j < NINDIRECT; j++)
					if(da[j])
						bfree(ip->dev, da[j]);
				brelse(dbp);
				bfree(ip->dev, a[i]);			
			}
		}
		brelse(bp);
		bfree(ip->dev, ip->addrs[NDIRECT+1]);
		ip->addrs[NDIRECT+1] = 0;
	}

	if (ip->addrs[NDIRECT+2]) {
		bp = bread(ip->dev, ip->addrs[NDIRECT+2]);
		a = (uint*)bp->data;

		for (i = 0; i < NINDIRECT; i++) {
			if (a[i]) {
				dbp = bread(ip->dev, a[i]);
				da = (uint*)dbp->data;

				for (j = 0; j < NINDIRECT; j++) {
					if (da[j]) {
						tbp = bread(ip->dev, da[j]);
						ta = (uint*)tbp->data;
					
						for (k = 0; k < NINDIRECT; k++) 
							if(ta[k])
								bfree(ip->dev, ta[k]);
						brelse(tbp);
						bfree(ip->dev, da[j]);
					}
				}
				
				brelse(dbp);
				bfree(ip->dev, a[i]);
			}
		}
		
		brelse(bp);
		bfree(ip->dev, ip->addrs[NDIRECT+2]);
		ip->addrs[NDIRECT+2] = 0;
	}

  ip->size = 0;
  iupdate(ip);
}
```

- `dbp, tbp` ๋ฅผ ์ ์ธํด์ 2์ฐจ, 3์ฐจ ๋ธ๋ญ์ ๊ฐ๋ฆฌํค๋๋ฐ ์ฐ๊ณ  `da, ta` ๋ฅผ ์ ์ธํด์ ํด๋น ๋ธ๋ญ์ data๋ฅผ ๊ฐ๋ฆฌํค๋๋ฐ ์ด๋ค.
- ipโaddrs์ indirect ์๋ฆฌ๋ 0์ผ๋ก ์ด๊ธฐํํด์ค๋ค.

### Update Parameters

- indirect blocks๋ฅผ ๊ตฌํํจ์ ๋ฐ๋ผ, write ๋ฅผ ์คํํ  ๋ ๋ ๋ง์ balloc ๊ณผ log_write๊ฐ ์คํ๋๋ฏ๋ก ๊ธฐ์กด parameter ๊ฐ์ผ๋ก๋ ๋ฒํผ ์บ์๊ฐ ๊ฝ์ฐจ๊ฒ ๋๊ณ , stress test์์ 'no buffer' panic์ด ๋ฐ์ํ๊ฒ ๋๋ค.
- ๋ฐ๋ผ์ MAXOPBLOCKS๋ฅผ ์ฌ์ ๋กญ๊ฒ ํ์ฅํด์ฃผ๊ณ , filewrite ํจ์์์ ์ฐ๊ธฐ ๊ฐ๋ฅํ ์ต๋๊ฐ ๋ํ ์์ ํด์ค๋ค.

```c
#define MAXOPBLOCKS 20 // max # of blocks any FS op writes
```

```c
int
filewrite(struct file *f, char *addr, int n)
{
			...
    int max = ((MAXOPBLOCKS-1-3-4) / 2) * 512;
	    ...
}
```

- filewrite์์ max ๊ฐ์ 2์ฐจ, 3์ฐจ ๋ธ๋ญ์ ์์ ํ  ๋์ ํ ๋นํ  ๋ 2๊ฐ์ฉ ํ์ํ๋ฏ๋ก ๊ธฐ์กด -1 -1 -2 ์์ -1 -3 -4๋ก ์์ ํ๋ค.

# Milestone 3

## ๐ฏ Implement the pread, pwrite system call

### system calls

`int pwrite(int fd, void* addr, int n, int off)`

- file์ offset๊ณผ ์๊ด์์ด parameterย ๋ก ์ฃผ์ด์ง off์ n๋งํผ ์ด๋ค.

`int pread(int fd, void* addr, int n, int off)`

- pwrite์ ๋ง์ฐฌ๊ฐ์ง๋ก ์ฃผ์ด์ง off์์ n๋งํผ ์ฝ์ด์จ๋ค.

### file.c

`filepread (struct file *f, char *addr, int n, int off)`

- fileread ํจ์์ ๋์ผํ๋, ํ์ผ์ ์ฝ์ด์ฌ ๋ offset์ ๋ณ๊ฒฝํ์ง ์๋๋ค.
- readi ํจ์๋ฅผ ์ด์ฉํด off ๋ถํฐ n ๋ฐ์ดํธ ์ฝ์ด์ค๊ณ , file์ offset์ ์์ ํ์ง ์๋๋ค.

```c
int
filepread(struct file *f, char *addr, int n, int off)
{
			...
    r = readi(f->ip, addr, off, n);
			...
}
```

`filepwrite (struct file *f, char *addr, int n, int off)`

- filewrite ํจ์์ ๋์ผํ์ง๋ง, ํ์ผ์ offset์ ๋ณ๊ฒฝํ์ง๋ ์๋๋ค.
- writei ํจ์๋ฅผ ์ด์ฉํด ์ต๋ max ๋งํผ off์ ์ฐ๊ณ , ํ๋ผ๋ฏธํฐ๋ก ์ ๋ฌ๋ off ๊ฐ์ ์์ง์ธ๋ค.

```c
int
filepwrite(struct file *f, char *addr, int n, int off)
{
			  ...
      if ((r = writei(f->ip, addr + i, off, n1)) > 0)
				off += r;
	      ...
}
```
