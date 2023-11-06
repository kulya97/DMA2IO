/***************************** Include Files *********************************/
#include "xaxidma.h"
#include "xparameters.h"
#include "xil_exception.h"
#include "xdebug.h"
#include "strings.h"
#include "xscugic.h"
#include "xuartps.h"
#include "xil_printf.h"
/******************** Constant Definitions **********************************/

#define DMA_DEV_ID		XPAR_AXIDMA_0_DEVICE_ID
#define RX_INTR_ID		XPAR_FABRIC_AXIDMA_0_S2MM_INTROUT_VEC_ID
#define TX_INTR_ID		XPAR_FABRIC_AXIDMA_0_MM2S_INTROUT_VEC_ID
#define INTC_DEVICE_ID          XPAR_SCUGIC_SINGLE_DEVICE_ID

/************************************************************/
//地址
#define MEM_BASE_ADDR		0x01000000

#define TX_BD_SPACE_BASE	(MEM_BASE_ADDR + 0x00100000)
#define TX_BD_SPACE_HIGH	(MEM_BASE_ADDR + 0x00FFFFFF)
#define RX_BD_SPACE_BASE	(MEM_BASE_ADDR)
#define RX_BD_SPACE_HIGH	(MEM_BASE_ADDR + 0x000FFFFF)

#define TX_BUFFER_BASE		(MEM_BASE_ADDR + 0x10000000)

#define RX_BUFFER_BASE		(MEM_BASE_ADDR + 0x03000000)
#define RX_BUFFER_HIGH		(MEM_BASE_ADDR + 0x04FFFFFF)

/************************************************************/
/*
 * AXI DMA操作需要先提供一个在内存中驻留的不变空间，用于存储需要进行的DMA操作。
 * 形容这“每一次操作”的东西叫做Buffer Descriptor，缩写叫BD，
 * 这些BD是连接成链表的形式的，因为BD会动态增加，而预先分配存储BD的空间是恒定的，
 * 因此BD被连成一个环（BD Ring）,其实就是一个循环链表。
 *
 * {packet1    },{packet2    },{packet3    }... n个pkt
 * {pkt len    },{pkt len    },{pkt len    }...每个pkt的大小由pktlen确定
 * {bd1,bd2,bd3},{bd1,bd2,bd3},{bd1,bd2,bd3}...每个pkt由n个bd组成，bd的数量
 *
 ************************************************************/
// 要传输的每个packet大小
//？？？！！！是否称为每个bd的内存大小更为合适
#define MAX_PKT_LEN		0x1000   //单位bit,改成4是不是会更加完美呢
// 每个packet对应的BD数量
#define NUMBER_OF_BDS_PER_PKT		128*0xff*0xff
// 一共要传输的packet个数
#define NUMBER_OF_PKTS_TO_TRANSFER 	0x1
// 总共需要的BD总数
#define NUMBER_OF_BDS_TO_TRANSFER	(NUMBER_OF_PKTS_TO_TRANSFER *NUMBER_OF_BDS_PER_PKT)
/************************************************************/
//超时计数
#define RESET_TIMEOUT_COUNTER	10000
/************************************************************/
/*中断合并阈值和延迟定时器阈值
 *有效范围为1到255
 *我们将合并阈值设置为包的总数。在这个例子中，接收端只会得到一个完成中断。
 */
#define COALESCING_COUNT		NUMBER_OF_PKTS_TO_TRANSFER
#define DELAY_TIMER_COUNT		100

#define INTC		XScuGic
#define INTC_HANDLER	XScuGic_InterruptHandler

/**************************** Type Definitions *******************************/
static void TxCallBack(XAxiDma_BdRing * TxRingPtr);
static void TxIntrHandler(void *Callback);
static void RxCallBack(XAxiDma_BdRing * RxRingPtr);
static void RxIntrHandler(void *Callback);

static int SetupIntrSystem(INTC * IntcInstancePtr, XAxiDma * AxiDmaPtr,
		u16 TxIntrId, u16 RxIntrId);
static void DisableIntrSystem(INTC * IntcInstancePtr, u16 TxIntrId,
		u16 RxIntrId);
static int RxSetup(XAxiDma * AxiDmaInstPtr);
static int TxSetup(XAxiDma * AxiDmaInstPtr);
static int SendPacket(XAxiDma * AxiDmaInstPtr);

/************************** Variable Definitions *****************************/
//设备实例
XAxiDma AxiDma;
//中断控制器实例
static INTC Intc;
//中断标志
volatile int TxDone;
volatile int RxDone;
volatile int Error;

//发送数据包缓冲区，必须32位对齐使用
u32 *Packet = (u32 *) TX_BUFFER_BASE;

void SetIntIO() { //设置积分时的io状态

}

void SetBuffInitConfig(u32 cycleCnt, u32 initCnt) {
	u32 bd_len = 0x1000; //单个bd的长度
	u32 bds_num = cycleCnt / bd_len; //需要多少个bd
	u32 end_bds_len = cycleCnt % bdlen; //最后一个bd的长度

	u32 bds_num;
	for (int pkts = 0; pkts < 12; pkt++) {
		for (int bds = 0; bds < 12; bds++) {

		}
	}

}

void GenerteChannelDataCycle(u32 *addr, u8 Channel, u32 highCnt, u32 lowCnt,
		u32 cycleCnt) {
	u32 * TxPacket = (u32 *) addr;
	if (FALSE) {
		xil_printf("校验地址错误，起始地址或终止地址超限");
		return;
	}
	if (Channel < 0 || Channel > 31) {
		xil_printf("通道选择错误！");
		return;
	}
	u32 wareCycle = lowCnt + highCnt; //波形周期
	u32 ClkCycle = wareCycle * cycleCnt; //波形周期数

	for (int i = 0; i < ClkCycle; i++) {
		if (i % (wareCycle) < highCnt)
			TxPacket[i] |= 1 << Channel;
		else {
			TxPacket[i] &= ~(1 << Channel);
		}
	}
}

void GenerteChannelDataHold(u32 *addr, u8 channel, u8 level, u32 clkCnt) {
	u32 * TxPacket = (u32 *) addr;
	if (FALSE) {
		xil_printf("校验地址错误，起始地址或终止地址超限");
		return;
	}
	if (channel < 0 || channel > 31) {
		xil_printf("通道选择错误！");
		return;
	}
	for (int i = 0; i < clkCnt; i++) {
		if (level)
			TxPacket[i] |= 1 << channel;
		else {
			TxPacket[i] &= ~(1 << channel);
		}
	}
}

void FlushData2DMA(void) {
	//发送之前刷新缓冲区
//	Xil_DCacheFlushRange((UINTPTR) TxPacket, MAX_PKT_LEN *
//	NUMBER_OF_BDS_TO_TRANSFER);
//	//
//	Xil_DCacheFlushRange((UINTPTR) RX_BUFFER_BASE, MAX_PKT_LEN *
//	NUMBER_OF_BDS_TO_TRANSFER);
}
/*****************************************************************************/
int main(void) {
	int Status;
	XAxiDma_Config *Config;
	xil_printf("\r\n--- Entering main() --- \r\n");
	xil_printf("njust cmos ate system v1.0\n");
	xil_printf("look up system config ...\n");
//查找设备配置信息
	Config = XAxiDma_LookupConfig(DMA_DEV_ID);
	if (!Config) {
		xil_printf("No config found for %d\r\n", DMA_DEV_ID);

		return XST_FAILURE;
	}

//初始化DMA引擎
	XAxiDma_CfgInitialize(&AxiDma, Config);

	if (!XAxiDma_HasSg(&AxiDma)) {
		xil_printf("Device configured as Simple mode \r\n");
		return XST_FAILURE;
	}
//设置发送通道，以使数据准备好发送
	Status = TxSetup(&AxiDma);
	if (Status != XST_SUCCESS) {

		xil_printf("Failed TX setup\r\n");
		return XST_FAILURE;
	}
//设置接收通道，以使数据准备好接收
	Status = RxSetup(&AxiDma);
	if (Status != XST_SUCCESS) {

		xil_printf("Failed RX setup\r\n");
		return XST_FAILURE;
	}
//设置中断
	Status = SetupIntrSystem(&Intc, &AxiDma, TX_INTR_ID, RX_INTR_ID);
	if (Status != XST_SUCCESS) {

		xil_printf("Failed intr setup\r\n");
		return XST_FAILURE;
	}
//初始化标志信号
	TxDone = 0;
	RxDone = 0;
	Error = 0;
//发送数据
	Status = SendPacket(&AxiDma);
	if (Status != XST_SUCCESS) {

		xil_printf("Failed send packet\r\n");
		return XST_FAILURE;
	}
	while (1) {
	}
	XAxiDma_Reset(&AxiDma);
	/* Disable TX and RX Ring interrupts and return success */
	DisableIntrSystem(&Intc, TX_INTR_ID, RX_INTR_ID);
	return XST_SUCCESS;
}
/*****************************************************************************/
static int RxSetup(XAxiDma * AxiDmaInstPtr) {
	XAxiDma_BdRing *RxRingPtr;
	int Status;
	XAxiDma_Bd BdTemplate;
	XAxiDma_Bd *BdPtr;
	XAxiDma_Bd *BdCurPtr;
	int BdCount;
	int FreeBdCount;
	UINTPTR RxBufferPtr;
	int Index;
//获取接收环
	RxRingPtr = XAxiDma_GetRxRing(&AxiDma);
//设置空间之前禁用所有读取中断
	XAxiDma_BdRingIntDisable(RxRingPtr, XAXIDMA_IRQ_ALL_MASK);
//设置bd空间
	BdCount = XAxiDma_BdRingCntCalc(XAXIDMA_BD_MINIMUM_ALIGNMENT,
			RX_BD_SPACE_HIGH - RX_BD_SPACE_BASE + 1);

//创建bd环
	Status = XAxiDma_BdRingCreate(RxRingPtr, RX_BD_SPACE_BASE,
	RX_BD_SPACE_BASE,
	XAXIDMA_BD_MINIMUM_ALIGNMENT, BdCount);
	if (Status != XST_SUCCESS) {
		xil_printf("Rx bd create failed with %d\r\n", Status);
		return XST_FAILURE;
	}
//为Rx通道设置BD模板。然后复制到每个RX BD。
//bd归零
	XAxiDma_BdClear(&BdTemplate);
//复制模板到创建的bd  模板为16个4字节数据uint32_t类型
	Status = XAxiDma_BdRingClone(RxRingPtr, &BdTemplate);
	if (Status != XST_SUCCESS) {
		xil_printf("Rx bd clone failed with %d\r\n", Status);
		return XST_FAILURE;
	}
//在读取bd环上加上缓冲区以便读取数据
	FreeBdCount = XAxiDma_BdRingGetFreeCnt(RxRingPtr);

	Status = XAxiDma_BdRingAlloc(RxRingPtr, FreeBdCount, &BdPtr);
	if (Status != XST_SUCCESS) {
		xil_printf("Rx bd alloc failed with %d\r\n", Status);
		return XST_FAILURE;
	}

	BdCurPtr = BdPtr;
	RxBufferPtr = RX_BUFFER_BASE;

	for (Index = 0; Index < FreeBdCount; Index++) {
		//设置bd缓冲地址
		Status = XAxiDma_BdSetBufAddr(BdCurPtr, RxBufferPtr);
		if (Status != XST_SUCCESS) {
			xil_printf("Rx set buffer addr %x on BD %x failed %d\r\n",
					(unsigned int) RxBufferPtr, (UINTPTR) BdCurPtr, Status);

			return XST_FAILURE;
		}
		//为给定的bd设置子段长度
		Status = XAxiDma_BdSetLength(BdCurPtr, MAX_PKT_LEN,
				RxRingPtr->MaxTransferLen);
		if (Status != XST_SUCCESS) {
			xil_printf("Rx set length %d on BD %x failed %d\r\n",
			MAX_PKT_LEN, (UINTPTR) BdCurPtr, Status);

			return XST_FAILURE;
		}
		//接收BDs不需要设置任何控件硬件会设置每个流的SOF/EOF位
		//设置bd控制位
		XAxiDma_BdSetCtrl(BdCurPtr, 0);
		//设置bd的id
		XAxiDma_BdSetId(BdCurPtr, RxBufferPtr);

		RxBufferPtr += MAX_PKT_LEN;
		BdCurPtr = (XAxiDma_Bd *) XAxiDma_BdRingNext(RxRingPtr, BdCurPtr);
	}

//设置合并阈值，因此只有一个接收中断在本例中出现如果你想有多个中断发生，改变 COALESCING_COUNT是一个较小的值
//为给定的描述符环形通道设置中断合并参数。
	Status = XAxiDma_BdRingSetCoalesce(RxRingPtr, COALESCING_COUNT,
	DELAY_TIMER_COUNT);
	if (Status != XST_SUCCESS) {
		xil_printf("Rx set coalesce failed with %d\r\n", Status);
		return XST_FAILURE;
	}

//将一组bd加入到分配的硬件中
	Status = XAxiDma_BdRingToHw(RxRingPtr, FreeBdCount, BdPtr);
	if (Status != XST_SUCCESS) {
		xil_printf("Rx ToHw failed with %d\r\n", Status);
		return XST_FAILURE;
	}

	/* Enable all RX interrupts */
	XAxiDma_BdRingIntEnable(RxRingPtr, XAXIDMA_IRQ_ALL_MASK);
	/* Enable Cyclic DMA mode */
	XAxiDma_BdRingEnableCyclicDMA(RxRingPtr);
	XAxiDma_SelectCyclicMode(AxiDmaInstPtr, XAXIDMA_DEVICE_TO_DMA, 1);

	/* Start RX DMA channel */
	Status = XAxiDma_BdRingStart(RxRingPtr);
	if (Status != XST_SUCCESS) {
		xil_printf("Rx start BD ring failed with %d\r\n", Status);
		return XST_FAILURE;
	}

	return XST_SUCCESS;
}

/*****************************************************************************/
static int TxSetup(XAxiDma * AxiDmaInstPtr) {
	XAxiDma_BdRing *TxRingPtr = XAxiDma_GetTxRing(&AxiDma);
	XAxiDma_Bd BdTemplate;
	int Status;

	/* Disable all TX interrupts before TxBD space setup */
	XAxiDma_BdRingIntDisable(TxRingPtr, XAXIDMA_IRQ_ALL_MASK);

	Status = XAxiDma_BdRingCreate(TxRingPtr, TX_BD_SPACE_BASE,
	TX_BD_SPACE_BASE,
	XAXIDMA_BD_MINIMUM_ALIGNMENT, NUMBER_OF_BDS_TO_TRANSFER);
	if (Status != XST_SUCCESS) {

		xil_printf("Failed create BD ring\r\n");
		return XST_FAILURE;
	}

	/*
	 * Like the RxBD space, we create a template and set all BDs to be the
	 * same as the template. The sender has to set up the BDs as needed.
	 */
	XAxiDma_BdClear(&BdTemplate);
	Status = XAxiDma_BdRingClone(TxRingPtr, &BdTemplate);
	if (Status != XST_SUCCESS) {

		xil_printf("Failed clone BDs\r\n");
		return XST_FAILURE;
	}

	/*
	 * Set the coalescing threshold, so only one transmit interrupt
	 * occurs for this example
	 *
	 * If you would like to have multiple interrupts to happen, change
	 * the COALESCING_COUNT to be a smaller value
	 */
	Status = XAxiDma_BdRingSetCoalesce(TxRingPtr, COALESCING_COUNT,
	DELAY_TIMER_COUNT);
	if (Status != XST_SUCCESS) {

		xil_printf("Failed set coalescing"
				" %d/%d\r\n", COALESCING_COUNT, DELAY_TIMER_COUNT);
		return XST_FAILURE;
	}

	/* Enable Cyclic DMA mode */
	XAxiDma_BdRingEnableCyclicDMA(TxRingPtr);
	XAxiDma_SelectCyclicMode(AxiDmaInstPtr, XAXIDMA_DMA_TO_DEVICE, 1);

	/* Enable all TX interrupts */
	XAxiDma_BdRingIntEnable(TxRingPtr, XAXIDMA_IRQ_ALL_MASK);

	/* Start the TX channel */
	Status = XAxiDma_BdRingStart(TxRingPtr);
	if (Status != XST_SUCCESS) {

		xil_printf("Failed bd start\r\n");
		return XST_FAILURE;
	}

	return XST_SUCCESS;
}

/*****************************************************************************/
static int SendPacket(XAxiDma * AxiDmaInstPtr) {
	XAxiDma_BdRing *TxRingPtr = XAxiDma_GetTxRing(AxiDmaInstPtr);
	u32 *TxPacket;
	u32 Value;
	XAxiDma_Bd *BdPtr, *BdCurPtr;
	int Status;
	int Index, Pkts;
	UINTPTR BufferAddr;
//读取单个pkt报文的最大长度

//限制单个pkt报文长度
//单个pkg长度要小于最大传输长度，bdlen*bdnum
//	if (MAX_PKT_LEN * NUMBER_OF_BDS_PER_PKT > TxRingPtr->MaxTransferLen) {
//
//		xil_printf("Invalid total per packet transfer length for the "
//				"packet %d/%d\r\n",
//		MAX_PKT_LEN * NUMBER_OF_BDS_PER_PKT, TxRingPtr->MaxTransferLen);
//
//		return XST_INVALID_PARAM;
//	}

	TxPacket = (u32 *) Packet;

	Value = 0x0;
//每个bd长度*总共的bd数
	for (Index = 0; Index < MAX_PKT_LEN * NUMBER_OF_BDS_TO_TRANSFER / 4;
			Index = Index + 1) {
		if (Index % 5 == 1 && Index < 4096 * 200)
			Value = 0xFFFFFFFF;
		else if (Index % 2 == 1 && Index >= 4096 * 200 && Index < 4096 * 500)
			Value = 0xFFFFFFFF;
		else if (Index % 200 == 1 && Index >= 4096 * 500 && Index < 4096 * 900)
			Value = 0xFFFFFFFF;
		else if (Index >= 4096 * 900 && Index < 4096 * 1200)
			Value = 0xFFFFFFFF;
		else if (Index >= 4096 * 1200 && Index < 4096 * 1500)
			Value = 0x0;
		else if (Index >= 4096 * 256 * 10 && Index < 4096 * 256 * 31)
			Value = 0xFFFFFFFF;
//				if (Index < 4096 * 1)
//					Value = 0xFFFFFFFF;
//		if (Index % 5 == 1)
//			Value = 0xFFFFFFFF;
//		else if (Index >= 300 && Index < 600)
//			Value = 0xFFFF;
		else
			Value = 0x0;
		TxPacket[Index] = Value;
	}
//发送之前刷新缓冲区
	Xil_DCacheFlushRange((UINTPTR) TxPacket, MAX_PKT_LEN *
	NUMBER_OF_BDS_TO_TRANSFER);
//
	Xil_DCacheFlushRange((UINTPTR) RX_BUFFER_BASE, MAX_PKT_LEN *
	NUMBER_OF_BDS_TO_TRANSFER);
//申请发送内存
	Status = XAxiDma_BdRingAlloc(TxRingPtr, NUMBER_OF_BDS_TO_TRANSFER, &BdPtr);
	if (Status != XST_SUCCESS) {

		xil_printf("Failed bd alloc\r\n");
		return XST_FAILURE;
	}

	BufferAddr = (UINTPTR) Packet;
	BdCurPtr = BdPtr;

	/*
	 * Set up the BD using the information of the packet to transmit
	 * Each transfer has NUMBER_OF_BDS_PER_PKT BDs
	 */
//遍历每一个pkt
	for (Index = 0; Index < NUMBER_OF_PKTS_TO_TRANSFER; Index++) {
		//遍历每个pkt下的每一个bd
		for (Pkts = 0; Pkts < NUMBER_OF_BDS_PER_PKT; Pkts++) {
			u32 CrBits = 0;
			//设置bd缓冲区地址
			Status = XAxiDma_BdSetBufAddr(BdCurPtr, BufferAddr);
			if (Status != XST_SUCCESS) {
				xil_printf("Tx set buffer addr %x on BD %x failed %d\r\n",
						(unsigned int) BufferAddr, (UINTPTR) BdCurPtr, Status);
				return XST_FAILURE;
			}
			//设置bd长度字段
//					if (Index == NUMBER_OF_PKTS_TO_TRANSFER - 1 && Pkts >= 1)
//						Status = XAxiDma_BdSetLength(BdCurPtr, 10,
//								TxRingPtr->MaxTransferLen);
//					else
//						Status = XAxiDma_BdSetLength(BdCurPtr, MAX_PKT_LEN,
//								TxRingPtr->MaxTransferLen);
			Status = XAxiDma_BdSetLength(BdCurPtr, MAX_PKT_LEN,
					TxRingPtr->MaxTransferLen);
			if (Status != XST_SUCCESS) {
				xil_printf("Tx set length %d on BD %x failed %d\r\n",
				MAX_PKT_LEN, (UINTPTR) BdCurPtr, Status);

				return XST_FAILURE;
			}

			if (Pkts == 0) {
				//起始bd设置sof
				CrBits |= XAXIDMA_BD_CTRL_TXSOF_MASK;
			}

			if (Pkts == (NUMBER_OF_BDS_PER_PKT - 1)) {

				//最后一个bd设置eof和ioc
				CrBits |= XAXIDMA_BD_CTRL_TXEOF_MASK;
			}

			XAxiDma_BdSetCtrl(BdCurPtr, CrBits);

			XAxiDma_BdSetId(BdCurPtr, BufferAddr);
			//移动bd指针到下一个bd位置
			BufferAddr += MAX_PKT_LEN;
			//依次添加bd到bdring
			BdCurPtr = (XAxiDma_Bd *) XAxiDma_BdRingNext(TxRingPtr, BdCurPtr);
		}
	}
	/* Give the BD to hardware */
	Status = XAxiDma_BdRingToHw(TxRingPtr, NUMBER_OF_BDS_TO_TRANSFER, BdPtr);
	if (Status != XST_SUCCESS) {

		xil_printf("Failed to hw, length %d\r\n",
				(int) XAxiDma_BdGetLength(BdPtr, TxRingPtr->MaxTransferLen));
		return XST_FAILURE;
	}

	return XST_SUCCESS;
}

/*****************************************************************************/
static void TxCallBack(XAxiDma_BdRing * TxRingPtr) {
	XAxiDma_Bd *BdPtr;

	/* Get all processed BDs from hardware */
	XAxiDma_BdRingFromHw(TxRingPtr, XAXIDMA_ALL_BDS, &BdPtr);
}

/*****************************************************************************/
static void TxIntrHandler(void *Callback) {
	XAxiDma_BdRing *TxRingPtr = (XAxiDma_BdRing *) Callback;
	u32 IrqStatus;
	int TimeOut;

	/* Read pending interrupts */
	IrqStatus = XAxiDma_BdRingGetIrq(TxRingPtr);

	/* Acknowledge pending interrupts */
	XAxiDma_BdRingAckIrq(TxRingPtr, IrqStatus);

	/* If no interrupt is asserted, we do not do anything
	 */
	if (!(IrqStatus & XAXIDMA_IRQ_ALL_MASK)) {

		return;
	}

	/*
	 * If error interrupt is asserted, raise error flag, reset the
	 * hardware to recover from the error, and return with no further
	 * processing.
	 */
	if ((IrqStatus & XAXIDMA_IRQ_ERROR_MASK)) {

		XAxiDma_BdRingDumpRegs(TxRingPtr);

		Error = 1;

		/*
		 * Reset should never fail for transmit channel
		 */
		XAxiDma_Reset(&AxiDma);

		TimeOut = RESET_TIMEOUT_COUNTER;

		while (TimeOut) {
			if (XAxiDma_ResetIsDone(&AxiDma)) {
				break;
			}

			TimeOut -= 1;
		}

		return;
	}

	/*
	 * If Transmit done interrupt is asserted, call TX call back function
	 * to handle the processed BDs and raise the according flag
	 */
	if ((IrqStatus & (XAXIDMA_IRQ_DELAY_MASK | XAXIDMA_IRQ_IOC_MASK))) {
		TxCallBack(TxRingPtr);
	}
}

/*****************************************************************************/
static void RxCallBack(XAxiDma_BdRing * RxRingPtr) {
	XAxiDma_Bd *BdPtr;
	/* Get finished BDs from hardware */
	XAxiDma_BdRingFromHw(RxRingPtr, XAXIDMA_ALL_BDS, &BdPtr);
}

/*****************************************************************************/
static void RxIntrHandler(void *Callback) {
	XAxiDma_BdRing *RxRingPtr = (XAxiDma_BdRing *) Callback;
	u32 IrqStatus;
	int TimeOut;

	/* Read pending interrupts */
	IrqStatus = XAxiDma_BdRingGetIrq(RxRingPtr);

	/* Acknowledge pending interrupts */
	XAxiDma_BdRingAckIrq(RxRingPtr, IrqStatus);

	/*
	 * If no interrupt is asserted, we do not do anything
	 */
	if (!(IrqStatus & XAXIDMA_IRQ_ALL_MASK)) {
		return;
	}

	/*
	 * If error interrupt is asserted, raise error flag, reset the
	 * hardware to recover from the error, and return with no further
	 * processing.
	 */
	if ((IrqStatus & XAXIDMA_IRQ_ERROR_MASK)) {

		XAxiDma_BdRingDumpRegs(RxRingPtr);

		Error = 1;

		/* Reset could fail and hang
		 * NEED a way to handle this or do not call it??
		 */
		XAxiDma_Reset(&AxiDma);

		TimeOut = RESET_TIMEOUT_COUNTER;

		while (TimeOut) {
			if (XAxiDma_ResetIsDone(&AxiDma)) {
				break;
			}

			TimeOut -= 1;
		}

		return;
	}

	/*
	 * If completion interrupt is asserted, call RX call back function
	 * to handle the processed BDs and then raise the according flag.
	 */
	if ((IrqStatus & (XAXIDMA_IRQ_DELAY_MASK | XAXIDMA_IRQ_IOC_MASK))) {
		RxCallBack(RxRingPtr);
	}
}

static int SetupIntrSystem(INTC * IntcInstancePtr, XAxiDma * AxiDmaPtr,
		u16 TxIntrId, u16 RxIntrId) {
	XAxiDma_BdRing *TxRingPtr = XAxiDma_GetTxRing(AxiDmaPtr);
	XAxiDma_BdRing *RxRingPtr = XAxiDma_GetRxRing(AxiDmaPtr);
	int Status;

	XScuGic_Config *IntcConfig;

	/*
	 * Initialize the interrupt controller driver so that it is ready to
	 * use.
	 */
	IntcConfig = XScuGic_LookupConfig(INTC_DEVICE_ID);
	if (NULL == IntcConfig) {
		return XST_FAILURE;
	}

	Status = XScuGic_CfgInitialize(IntcInstancePtr, IntcConfig,
			IntcConfig->CpuBaseAddress);
	if (Status != XST_SUCCESS) {
		return XST_FAILURE;
	}

	XScuGic_SetPriorityTriggerType(IntcInstancePtr, TxIntrId, 0xA0, 0x3);

	XScuGic_SetPriorityTriggerType(IntcInstancePtr, RxIntrId, 0xA0, 0x3);
	/*
	 * Connect the device driver handler that will be called when an
	 * interrupt for the device occurs, the handler defined above performs
	 * the specific interrupt processing for the device.
	 */
	Status = XScuGic_Connect(IntcInstancePtr, TxIntrId,
			(Xil_InterruptHandler) TxIntrHandler, TxRingPtr);
	if (Status != XST_SUCCESS) {
		return Status;
	}

	Status = XScuGic_Connect(IntcInstancePtr, RxIntrId,
			(Xil_InterruptHandler) RxIntrHandler, RxRingPtr);
	if (Status != XST_SUCCESS) {
		return Status;
	}

	XScuGic_Enable(IntcInstancePtr, TxIntrId);
	XScuGic_Enable(IntcInstancePtr, RxIntrId);

	/* Enable interrupts from the hardware */

	Xil_ExceptionInit();
	Xil_ExceptionRegisterHandler(XIL_EXCEPTION_ID_INT,
			(Xil_ExceptionHandler) INTC_HANDLER, (void *) IntcInstancePtr);

	Xil_ExceptionEnable()
	;
	return XST_SUCCESS;
}

static void DisableIntrSystem(INTC * IntcInstancePtr, u16 TxIntrId,
		u16 RxIntrId) {
	XScuGic_Disconnect(IntcInstancePtr, TxIntrId);
	XScuGic_Disconnect(IntcInstancePtr, RxIntrId);
}

