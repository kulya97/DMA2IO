/***************************** Include Files *********************************/
#include "xaxidma.h"
#include "xparameters.h"
#include "xil_exception.h"
#include "xdebug.h"
#include "strings.h"
extern void xil_printf(const char *format, ...);
#include "xscugic.h"

/******************** Constant Definitions **********************************/

#define DMA_DEV_ID		XPAR_AXIDMA_0_DEVICE_ID
#define RX_INTR_ID		XPAR_FABRIC_AXIDMA_0_S2MM_INTROUT_VEC_ID
#define TX_INTR_ID		XPAR_FABRIC_AXIDMA_0_MM2S_INTROUT_VEC_ID
#define INTC_DEVICE_ID          XPAR_SCUGIC_SINGLE_DEVICE_ID

//#warning CHECK FOR THE VALID DDR ADDRESS IN XPARAMETERS.H, \
//			DEFAULT SET TO 0x01000000
/************************************************************/
//��ַ
#define MEM_BASE_ADDR		0x01000000

#define TX_BD_SPACE_BASE	(MEM_BASE_ADDR + 0x00100000)
#define TX_BD_SPACE_HIGH	(MEM_BASE_ADDR + 0x00FFFFFF)

#define TX_BUFFER_BASE		(MEM_BASE_ADDR + 0x10000000)

#define RX_BD_SPACE_BASE	(MEM_BASE_ADDR)
#define RX_BD_SPACE_HIGH	(MEM_BASE_ADDR + 0x000FFFFF)
#define RX_BUFFER_BASE		(MEM_BASE_ADDR + 0x03000000)
#define RX_BUFFER_HIGH		(MEM_BASE_ADDR + 0x04FFFFFF)

/************************************************************/
/*
 * AXI DMA������Ҫ���ṩһ�����ڴ���פ���Ĳ���ռ䣬���ڴ洢��Ҫ���е�DMA������
 * �����⡰ÿһ�β������Ķ�������Buffer Descriptor����д��BD��
 * ��ЩBD�����ӳ��������ʽ�ģ���ΪBD�ᶯ̬���ӣ���Ԥ�ȷ���洢BD�Ŀռ��Ǻ㶨�ģ�
 * ���BD������һ������BD Ring��,��ʵ����һ��ѭ������
 *
 * {packet1    },{packet2    },{packet3    }... n��pkt
 * {pkt len    },{pkt len    },{pkt len    }...ÿ��pkt�Ĵ�С��pktlenȷ��
 * {bd1,bd2,bd3},{bd1,bd2,bd3},{bd1,bd2,bd3}...ÿ��pkt��n��bd��ɣ�bd������
 *
 ************************************************************/
// Ҫ�����ÿ��packet��С
//�������������Ƿ��Ϊÿ��bd���ڴ��С��Ϊ����
#define MAX_PKT_LEN		0x1000   //��λbit
// ÿ��packet��Ӧ��BD����
#define NUMBER_OF_BDS_PER_PKT		128
// һ��Ҫ�����packet����
#define NUMBER_OF_PKTS_TO_TRANSFER 	0xFF
// �ܹ���Ҫ��BD����
#define NUMBER_OF_BDS_TO_TRANSFER	(NUMBER_OF_PKTS_TO_TRANSFER *NUMBER_OF_BDS_PER_PKT)
/************************************************************/
//��ʱ����
#define RESET_TIMEOUT_COUNTER	10000
/************************************************************/
/*�жϺϲ���ֵ���ӳٶ�ʱ����ֵ
 *��Ч��ΧΪ1��255
 *���ǽ��ϲ���ֵ����Ϊ��������������������У����ն�ֻ��õ�һ������жϡ�
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
//�豸ʵ��
XAxiDma AxiDma;
//�жϿ�����ʵ��
static INTC Intc;
//�жϱ�־
volatile int TxDone;
volatile int RxDone;
volatile int Error;

//�������ݰ�������������32λ����ʹ��
u32 *Packet = (u32 *) TX_BUFFER_BASE;

u8 *txchar = "C100(C200(C300(H10L20),C200(H20L10)),C100(H1L5))";

void tx_tg(u8 *txchar) {

}

/*****************************************************************************/

int main(void) {
	int Status;
	XAxiDma_Config *Config;
	xil_printf("\r\n--- Entering main() --- \r\n");

	//�����豸������Ϣ
	Config = XAxiDma_LookupConfig(DMA_DEV_ID);
	if (!Config) {
		xil_printf("No config found for %d\r\n", DMA_DEV_ID);

		return XST_FAILURE;
	}

	//��ʼ��DMA����
	XAxiDma_CfgInitialize(&AxiDma, Config);

	if (!XAxiDma_HasSg(&AxiDma)) {
		xil_printf("Device configured as Simple mode \r\n");
		return XST_FAILURE;
	}
	//���÷���ͨ������ʹ����׼���÷���
	Status = TxSetup(&AxiDma);
	if (Status != XST_SUCCESS) {

		xil_printf("Failed TX setup\r\n");
		return XST_FAILURE;
	}
	//���ý���ͨ������ʹ����׼���ý���
	Status = RxSetup(&AxiDma);
	if (Status != XST_SUCCESS) {

		xil_printf("Failed RX setup\r\n");
		return XST_FAILURE;
	}
	//�����ж�
	Status = SetupIntrSystem(&Intc, &AxiDma, TX_INTR_ID, RX_INTR_ID);
	if (Status != XST_SUCCESS) {

		xil_printf("Failed intr setup\r\n");
		return XST_FAILURE;
	}
	//��ʼ����־�ź�
	TxDone = 0;
	RxDone = 0;
	Error = 0;
	//��������
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
	//��ȡ���ջ�
	RxRingPtr = XAxiDma_GetRxRing(&AxiDma);
	//���ÿռ�֮ǰ�������ж�ȡ�ж�
	XAxiDma_BdRingIntDisable(RxRingPtr, XAXIDMA_IRQ_ALL_MASK);
	//����bd�ռ�
	BdCount = XAxiDma_BdRingCntCalc(XAXIDMA_BD_MINIMUM_ALIGNMENT,
			RX_BD_SPACE_HIGH - RX_BD_SPACE_BASE + 1);

	//����bd��
	Status = XAxiDma_BdRingCreate(RxRingPtr, RX_BD_SPACE_BASE,
	RX_BD_SPACE_BASE,
	XAXIDMA_BD_MINIMUM_ALIGNMENT, BdCount);
	if (Status != XST_SUCCESS) {
		xil_printf("Rx bd create failed with %d\r\n", Status);
		return XST_FAILURE;
	}
	//ΪRxͨ������BDģ�塣Ȼ���Ƶ�ÿ��RX BD��
	//bd����
	XAxiDma_BdClear(&BdTemplate);
	//����ģ�嵽������bd  ģ��Ϊ16��4�ֽ�����uint32_t����
	Status = XAxiDma_BdRingClone(RxRingPtr, &BdTemplate);
	if (Status != XST_SUCCESS) {
		xil_printf("Rx bd clone failed with %d\r\n", Status);
		return XST_FAILURE;
	}
	//�ڶ�ȡbd���ϼ��ϻ������Ա��ȡ����
	FreeBdCount = XAxiDma_BdRingGetFreeCnt(RxRingPtr);

	Status = XAxiDma_BdRingAlloc(RxRingPtr, FreeBdCount, &BdPtr);
	if (Status != XST_SUCCESS) {
		xil_printf("Rx bd alloc failed with %d\r\n", Status);
		return XST_FAILURE;
	}

	BdCurPtr = BdPtr;
	RxBufferPtr = RX_BUFFER_BASE;

	for (Index = 0; Index < FreeBdCount; Index++) {
		//����bd�����ַ
		Status = XAxiDma_BdSetBufAddr(BdCurPtr, RxBufferPtr);
		if (Status != XST_SUCCESS) {
			xil_printf("Rx set buffer addr %x on BD %x failed %d\r\n",
					(unsigned int) RxBufferPtr, (UINTPTR) BdCurPtr, Status);

			return XST_FAILURE;
		}
		//Ϊ������bd�����Ӷγ���
		Status = XAxiDma_BdSetLength(BdCurPtr, MAX_PKT_LEN,
				RxRingPtr->MaxTransferLen);
		if (Status != XST_SUCCESS) {
			xil_printf("Rx set length %d on BD %x failed %d\r\n",
			MAX_PKT_LEN, (UINTPTR) BdCurPtr, Status);

			return XST_FAILURE;
		}
		//����BDs����Ҫ�����κοؼ�Ӳ��������ÿ������SOF/EOFλ
		//����bd����λ
		XAxiDma_BdSetCtrl(BdCurPtr, 0);
		//����bd��id
		XAxiDma_BdSetId(BdCurPtr, RxBufferPtr);

		RxBufferPtr += MAX_PKT_LEN;
		BdCurPtr = (XAxiDma_Bd *) XAxiDma_BdRingNext(RxRingPtr, BdCurPtr);
	}

	//���úϲ���ֵ�����ֻ��һ�������ж��ڱ����г�����������ж���жϷ������ı� COALESCING_COUNT��һ����С��ֵ
	//Ϊ����������������ͨ�������жϺϲ�������
	Status = XAxiDma_BdRingSetCoalesce(RxRingPtr, COALESCING_COUNT,
	DELAY_TIMER_COUNT);
	if (Status != XST_SUCCESS) {
		xil_printf("Rx set coalesce failed with %d\r\n", Status);
		return XST_FAILURE;
	}

	//��һ��bd���뵽�����Ӳ����
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
	//��ȡ����pkt���ĵ���󳤶�
	u32 maxtransferlen = TxRingPtr->MaxTransferLen;

	//���Ƶ���pkt���ĳ���
	//����pkg����ҪС������䳤�ȣ�bdlen*bdnum
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
	//ÿ��bd����*�ܹ���bd��
	for (Index = 0; Index < MAX_PKT_LEN * NUMBER_OF_BDS_TO_TRANSFER / 4;
			Index = Index + 1) {
		if (Index % 5 == 1 && Index < 4096*200)
			Value = 0xFFFFFFFF;
		else if (Index % 2 == 1 && Index >= 4096*200 && Index < 4096*500)
			Value = 0xFFFFFFFF;
		else if (Index % 200 == 1 && Index >= 4096*500 && Index < 4096*900)
			Value = 0xFFFFFFFF;
		else if (Index >= 4096*900 && Index < 4096*1200)
			Value = 0xFFFFFFFF;
		else if (Index >= 4096*1200 && Index < 4096*1500)
			Value = 0x0;
		else if (Index >= 4096*256*10 && Index < 4096*256*31)
			Value = 0xFFFFFFFF;
//		if (Index % 5 == 1)
//			Value = 0xFFFFFFFF;
//		else if (Index >= 300 && Index < 600)
//			Value = 0xFFFF;
		else
			Value = 0x0;
		TxPacket[Index] = Value;
	}
	//����֮ǰˢ�»�����
	Xil_DCacheFlushRange((UINTPTR) TxPacket, MAX_PKT_LEN *
	NUMBER_OF_BDS_TO_TRANSFER);
	//
	Xil_DCacheFlushRange((UINTPTR) RX_BUFFER_BASE, MAX_PKT_LEN *
	NUMBER_OF_BDS_TO_TRANSFER);
	//���뷢���ڴ�
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
	//����ÿһ��pkt
	for (Index = 0; Index < NUMBER_OF_PKTS_TO_TRANSFER; Index++) {
		//����ÿ��pkt�µ�ÿһ��bd
		for (Pkts = 0; Pkts < NUMBER_OF_BDS_PER_PKT; Pkts++) {
			u32 CrBits = 0;
			//����bd��������ַ
			Status = XAxiDma_BdSetBufAddr(BdCurPtr, BufferAddr);
			if (Status != XST_SUCCESS) {
				xil_printf("Tx set buffer addr %x on BD %x failed %d\r\n",
						(unsigned int) BufferAddr, (UINTPTR) BdCurPtr, Status);
				return XST_FAILURE;
			}
			//����bd�����ֶ�
			Status = XAxiDma_BdSetLength(BdCurPtr, MAX_PKT_LEN,
					TxRingPtr->MaxTransferLen);
			if (Status != XST_SUCCESS) {
				xil_printf("Tx set length %d on BD %x failed %d\r\n",
				MAX_PKT_LEN, (UINTPTR) BdCurPtr, Status);

				return XST_FAILURE;
			}

			if (Pkts == 0) {
				//��ʼbd����sof
				CrBits |= XAXIDMA_BD_CTRL_TXSOF_MASK;
			}

			if (Pkts == (NUMBER_OF_BDS_PER_PKT - 1)) {

				//���һ��bd����eof��ioc
				CrBits |= XAXIDMA_BD_CTRL_TXEOF_MASK;
			}

			XAxiDma_BdSetCtrl(BdCurPtr, CrBits);

			XAxiDma_BdSetId(BdCurPtr, BufferAddr);
			//�ƶ�bdָ�뵽��һ��bdλ��
			BufferAddr += MAX_PKT_LEN;
			//�������bd��bdring
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
