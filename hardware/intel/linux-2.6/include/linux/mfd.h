/*
*  add by lwl for brcm bluesleep driver 
*
*/


#define  MFD_HSU_A0_STEPPING	1

#define chan_readl(chan, offset)	readl(chan->reg + offset)
#define chan_writel(chan, offset, val)	writel(val, chan->reg + offset)

#define mfd_readl(obj, offset)		readl(obj->reg + offset)
#define mfd_writel(obj, offset, val)	writel(val, obj->reg + offset)

#define HSU_DMA_TIMEOUT_CHECK_FREQ	(HZ/100)

struct hsu_dma_buffer {
	u8		*buf;
	dma_addr_t	dma_addr;
	u32		dma_size;
	u32		ofs;
};

struct hsu_dma_chan {
	u32	id;
	enum dma_data_direction	dirt;
	struct uart_hsu_port	*uport;
	void __iomem		*reg;
};

/* max queue before HSU pm active */
#define MAXQ	2048
#define CMD_WL	1
#define CMD_WB	2
#define CMD_TX	3
#define CMD_RX_STOP	4
#define CMD_TX_STOP	5

/* to record the pin wakeup states */
#define PM_WAKEUP	1

struct uart_hsu_port {
	struct uart_port        port;
	unsigned char           ier;
	unsigned char           lcr;
	unsigned char           mcr;
	unsigned int            lsr_break_flag;
	char			name[12];
	int			index;
	struct device		*dev;

	unsigned int		tx_addr;
	struct hsu_dma_chan	*txc;
	struct hsu_dma_chan	*rxc;
	struct hsu_dma_buffer	txbuf;
	struct hsu_dma_buffer	rxbuf;
	int			use_dma;	/* flag for DMA/PIO */
	int			running;
	int			dma_tx_on;
	int			dma_rx_on;
	char			reg_shadow[HSU_PORT_REG_LENGTH];
	struct circ_buf		qcirc;
	int			qbuf[MAXQ * 3]; /* cmd + offset + value */
	struct work_struct	qwork;
	spinlock_t		qlock;
	unsigned long		pm_flags;
	/* hsu_dma_rx_tasklet is used to displace specific treatment
	 * from dma_chan_irq IRQ handler into the tasklet callback,
	 * hence enabling low latency mode for hsu dma (forbidden
	 * in IRQ context) */
	struct tasklet_struct	hsu_dma_rx_tasklet;
	int			suspended;
		int			reopen;

};

/* Top level data structure of HSU */
struct hsu_port {
	void __iomem	*reg;
	unsigned long	paddr;
	unsigned long	iolen;
	u32		irq;
	struct uart_hsu_port	port[4];
	struct hsu_dma_chan	chans[10];

	struct dentry *debugfs;
};
