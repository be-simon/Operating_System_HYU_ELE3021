# Milestone 1

## 🎯 Implement sync system call

### log.c

`int sync (void)`

- 버퍼 데이터들의 변경사항을 log에 써주고 disk에 반영한다 (commit)
- 만약 커밋 중이거나 log space가 사용 중이라면 sleep 상태로 기다린다.

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

- 로그 헤더에 변경된 블럭의 수를 리턴한다.

```c
int 
get_log_num(void)
{
	return log.lh.n; 
}
```

`void end_op (void)`

- write를 할 때 바로 커밋하지 않고 버퍼에만 기록해둔 후, sync 시스템 콜 호출 또는 log space가 가득 찼을 때 디스크에 써야하므로 로그가 찼을 때만 커밋을 하도록 변경해준다.

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

## 🎯 Expand the maximum size of a file (Triple indirect block 구현)

### fs.h

- Triple indirect로 변경하면서 `NDIRECT`의 수도 줄어야한다.
- 대신 `NINDERECT` 의 제곱인  `NDINDIRECT` 와 세제곱인 `NTINDIRECT`를 새로 선언한다.
- `MAXFILE` 의 값에 추가된 indirect block들 만큼 더해준다.

```c
#define NDIRECT 10
#define NINDIRECT (BSIZE / sizeof(uint))
#define NDINDIRECT (NINDIRECT * NINDIRECT)
#define NTINDIRECT (NDINDIRECT * NINDIRECT)
#define MAXFILE (NDIRECT + NINDIRECT +NDINDIRECT + NTINDIRECT)
```

- `dinode` 구조체의 `addrs`도 변경해준다.

```c
struct dinode {
	...
	uint addrs[NDIRECT+3];   // Data block addresses
};
```

### file.h

- `dinode`와 마찬가지로 `inode` 구조체에서도 `addrs`을 수정해준다.

```c
struct inode {
  ...
  uint addrs[NDIRECT+3];
};
```

### fs.c

`static uint bmap (struct inode *ip, uint bn)`

- 기존의 single indirect block을 읽는 것에 더해서, 그 이상의 블럭 넘버에 대해 2차 및 3차 블럭을 읽어오도록 수정한다.

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

- `dbn` 과 `tbn` 을 선언해서 2차, 3차 블럭에서 몇번째 엔트리에 접근해야하는지 저장해둔다.

`static void itrunc (struct inode *ip)`

- 2차, 3차까지 블럭이 형성되어있을 수 있으므로, 2차 및 3차 블럭들도 확인 후 블럭을 해제해준다.
- 각 iteration에서 할당된 블럭이 있는지 확인하고 있다면 free 해준다.

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

- `dbp, tbp` 를 선언해서 2차, 3차 블럭을 가리키는데 쓰고 `da, ta` 를 선언해서 해당 블럭의 data를 가리키는데 쓴다.
- ip→addrs의 indirect 자리는 0으로 초기화해준다.

### Update Parameters

- indirect blocks를 구현함에 따라, write 를 실행할 때 더 많은 balloc 과 log_write가 실행되므로 기존 parameter 값으로는 버퍼 캐시가 꽉차게 되고, stress test에서 'no buffer' panic이 발생하게 된다.
- 따라서 MAXOPBLOCKS를 여유롭게 확장해주고, filewrite 함수에서 쓰기 가능한 최대값 또한 수정해준다.

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

- filewrite에서 max 값은 2차, 3차 블럭을 수정할 때와 할당할 때 2개씩 필요하므로 기존 -1 -1 -2 에서 -1 -3 -4로 수정한다.

# Milestone 3

## 🎯 Implement the pread, pwrite system call

### system calls

`int pwrite(int fd, void* addr, int n, int off)`

- file의 offset과 상관없이 parameter 로 주어진 off에 n만큼 쓴다.

`int pread(int fd, void* addr, int n, int off)`

- pwrite와 마찬가지로 주어진 off에서 n만큼 읽어온다.

### file.c

`filepread (struct file *f, char *addr, int n, int off)`

- fileread 함수와 동일하나, 파일을 읽어올 때 offset을 변경하지 않는다.
- readi 함수를 이용해 off 부터 n 바이트 읽어오고, file의 offset은 수정하지 않는다.

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

- filewrite 함수와 동일하지만, 파일의 offset을 변경하지는 않는다.
- writei 함수를 이용해 최대 max 만큼 off에 쓰고, 파라미터로 전달된 off 값을 움직인다.

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
