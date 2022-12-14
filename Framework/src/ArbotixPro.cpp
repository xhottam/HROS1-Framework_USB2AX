/*
 *   ArbotixPro.cpp
 *
 *   Author: ROBOTIS
 *
 */
#include <stdio.h>
#include "FSR.h"
#include "ArbotixPro.h"
#include "MotionStatus.h"
#include <stdlib.h>

using namespace Robot;

#define ID		         	(2)
#define LENGTH				(3)
#define INSTRUCTION			(4)
#define ERRBIT				(4)
#define PARAMETER			(5)
#define DEFAULT_BAUDNUMBER	        (1)

#define INST_PING			(1)
#define INST_READ			(2)
#define INST_WRITE			(3)
#define INST_REG_WRITE		(4)
#define INST_ACTION			(5)
#define INST_RESET			(6)
#define INST_SYNC_WRITE		(131)   // 0x83
#define INST_SYNC_READ      (132)   // 0x84
#define INST_BULK_READ      (146)   // 0x92


BulkReadData::BulkReadData() :
	start_address(0),
	length(0),
	error(-1)
{
	for (int i = 0; i < AXDXL::MAXNUM_ADDRESS; i++)
		table[i] = 0;
}

int BulkReadData::ReadByte(int address)
{
	if (address >= start_address && address < (start_address + length))
		return (int)table[address];

	return 0;
}

int BulkReadData::ReadWord(int address)
{
	if (address >= start_address && address < (start_address + length))
		return ArbotixPro::MakeWord(table[address], table[address + 1]);

	return 0;
}


ArbotixPro::ArbotixPro(PlatformArbotixPro *platform)
{
	m_Platform = platform;
	DEBUG_PRINT = false;
	m_DelayedWords = 0;
	m_bIncludeTempData = false;
	m_BulkReadTxPacket[LENGTH] = 0;
	for (int i = 0; i < ID_BROADCAST; i++){
		m_BulkReadData[i] = BulkReadData();
        }
}

ArbotixPro::~ArbotixPro()
{
	Disconnect();
	exit(0);
}

int ArbotixPro::TxRxSyncPacket(unsigned char *txpacket, unsigned char *rxpacket, int priority){

        int to_length = 0;
        int res = TX_FAIL;
        int length = txpacket[LENGTH] + 4;
	int num_servos = (length - ( PARAMETER  + 2 )) - 1;


        if (m_Platform->WritePort(txpacket, length) == length){                        
                        m_Platform->SetPacketTimeout(length);
                        
                        for (int x = 0; x < num_servos; x++)
                            {
                                  int _id   = txpacket[PARAMETER + x + 2];
                                  int _len  = txpacket[PARAMETER + 1];
                                  int _addr = txpacket[PARAMETER];

                                  to_length += _len;
                                  m_BulkReadData[_id].length = _len;
                                  m_BulkReadData[_id].start_address = _addr;
				  m_BulkReadData[_id].error = -1;

                            }
			int get_length = 0;
                        to_length += 6;
                        if (DEBUG_PRINT == true)
                        	fprintf(stderr, "\nRX: ");
                        while (1)
                            {
									length = m_Platform->ReadPort(&rxpacket[get_length], to_length - get_length);
									if (DEBUG_PRINT == true)
										{
											for (int n = 0; n < length; n++)
												fprintf(stderr, "%.2X ", rxpacket[get_length + n]);
										}
									get_length += length;

									if (get_length == to_length)
										{
											// Find packet header
											int i;
											for (i = 0; i < (get_length - 1); i++)
												{
													if (rxpacket[i] == 0xFF && rxpacket[i + 1] == 0xFF)
														break;
													else if (i == (get_length - 2) && rxpacket[get_length - 1] == 0xFF)
														break;
												}

											if (i == 0)
												{
													// Check checksum
													unsigned char checksum = CalculateChecksum(rxpacket);
													if (DEBUG_PRINT == true)
														fprintf(stderr, "CHK:%.2X\n", checksum);

													if (rxpacket[get_length - 1] == checksum)
														{
											                        for (int x = 0; x < num_servos; x++)
                       												     {
                                  											int _id   = txpacket[PARAMETER + x + 2];
                                  											int _len  = txpacket[PARAMETER + 1];
															for (int j = 0; j < _len; j++)
															{
															m_BulkReadData[_id].table[m_BulkReadData[_id].start_address + j] = rxpacket[PARAMETER + j];
									                                                m_BulkReadData[_id].error = (int)rxpacket[ERRBIT];
                                                                                                                        //fprintf(stderr, "ID:%u, temp:%u\n", _id,rxpacket[PARAMETER + j]);
															}
                            										              }
															res = SUCCESS;
														}
													else
														res = RX_CORRUPT;

													break;
												}
											else
												{
													for (int j = 0; j < (get_length - i); j++)
														rxpacket[j] = rxpacket[j + i];
													get_length -= i;
												}
										}
									else
										{
											if (m_Platform->IsPacketTimeout() == true)
												{
													if (get_length == 0)
														res = RX_TIMEOUT;
													else
														res = RX_CORRUPT;

													break;
												}
										}
                            }
        }

        return res;

}

int ArbotixPro::TxRxBulkPacket(unsigned char *txpacket, unsigned char *rxpacket, int priority){


	int to_length = 0; 
	int res = TX_FAIL;
	int length = txpacket[LENGTH] + 4;


        int nb_servos = (txpacket[LENGTH] - 3) / 3; 


        for (int n = 0; n < nb_servos; n++)
        {
	  int _id   = txpacket[PARAMETER + (3 * n) + 2];
          int _len  = txpacket[PARAMETER + (3 * n) + 1];
	  int _addr = txpacket[PARAMETER + (3 * n) + 3];	

          to_length = _len + 6;

          m_BulkReadData[_id].length = _len;
	  m_BulkReadData[_id].start_address = _addr;

	  unsigned char _txpacket[MAXNUM_TXPARAM + 10] = {0, };
          int _length = 4 + 4; 
 	  _txpacket[0] = 0xFF;
          _txpacket[1] = 0xFF;

          _txpacket[ID]             = _id;
          _txpacket[INSTRUCTION]    = INST_READ;
          _txpacket[PARAMETER]      = (unsigned char)_addr;
          _txpacket[PARAMETER + 1]  = (unsigned char)_len;
          _txpacket[LENGTH]         = 4;
          _txpacket[_length - 1] = CalculateChecksum(_txpacket);


           if (m_Platform->WritePort(_txpacket, _length) == _length)
           {
                m_Platform->FlushPort();
	        m_Platform->SetPacketTimeout(_length);
                int get_length = 0;
                if (DEBUG_PRINT == true)
                	fprintf(stderr, "\nRX: ");
                while (1)
                    {
                        length = m_Platform->ReadPort(&rxpacket[get_length], to_length - get_length);
                        if (DEBUG_PRINT == true)
                            {
                        for (int n = 0; n < length; n++)
                            fprintf(stderr, "%.2X ", rxpacket[get_length + n]);
                        }
                        get_length += length;

                        if (get_length == to_length)
                        {
                            m_BulkReadData[_id].error = -1;
                            // Find packet header
                            int i;
                            for (i = 0; i < (get_length - 1); i++)
                            {
                                if (rxpacket[i] == 0xFF && rxpacket[i + 1] == 0xFF)
                                    break;
                                else if (i == (get_length - 2) && rxpacket[get_length - 1] == 0xFF)
                                    break;
                            }

                            if (i == 0)
                                {
                                    // Check checksum
                                    unsigned char checksum = CalculateChecksum(rxpacket);
                                    if (DEBUG_PRINT == true)
                                        fprintf(stderr, "CHK:%.2X\n", checksum);
                                    if (rxpacket[get_length - 1] == checksum)
                                        {
                                            if (DEBUG_PRINT == true)
                                        	fprintf(stderr, "ID:%u\n", rxpacket[ID]);
                                            for (int j = 0; j < (rxpacket[LENGTH] - 2); j++)
                                                {
                                                    m_BulkReadData[rxpacket[ID]].table[m_BulkReadData[rxpacket[ID]].start_address + j] = rxpacket[PARAMETER + j];
                                                    m_BulkReadData[rxpacket[ID]].error = (int)rxpacket[ERRBIT];
                                                    //fprintf(stderr, "%.2X\n ", m_BulkReadData[rxpacket[ID]].table[m_BulkReadData[rxpacket[ID]].start_address + j]);   
                                                }
                                            res = SUCCESS;
                                        }
                                    else
                                        res = RX_CORRUPT;

                                    break;
                                }
                            else
                                {
                                    for (int j = 0; j < (get_length - i); j++)
                                        rxpacket[j] = rxpacket[j + i];
                                    get_length -= i;
                                 }
                        }
                        else
                        {
                            if (m_Platform->IsPacketTimeout() == true)
                                {
                                    if (get_length == 0)
                                        res = RX_TIMEOUT;
                                    else
                                        res = RX_CORRUPT;

                                    break;
                                }
                        }
				    }
           } 
        }

	return res;
}

int ArbotixPro::TxRxPacket(unsigned char *txpacket, unsigned char *rxpacket, int priority, bool transformToINST_READ)
{
	if (priority > 1)
		m_Platform->LowPriorityWait();
	if (priority > 0)
		m_Platform->MidPriorityWait();
	m_Platform->HighPriorityWait();

	int res = TX_FAIL;
	int length = txpacket[LENGTH] + 4;

	txpacket[0] = 0xFF;
	txpacket[1] = 0xFF;
	txpacket[length - 1] = CalculateChecksum(txpacket);

	if (DEBUG_PRINT == true)
		{
			fprintf(stderr, "\nTX: ");
			for (int n = 0; n < length; n++)
				fprintf(stderr, "%.2X ", txpacket[n]);

			fprintf(stderr, "INST: ");
			switch (txpacket[INSTRUCTION])
				{
				case INST_PING:
					fprintf(stderr, "PING\n");
					break;

				case INST_READ:
					fprintf(stderr, "READ\n");
					break;

				case INST_WRITE:
					fprintf(stderr, "WRITE\n");
					break;

				case INST_REG_WRITE:
					fprintf(stderr, "REG_WRITE\n");
					break;

				case INST_ACTION:
					fprintf(stderr, "ACTION\n");
					break;

				case INST_RESET:
					fprintf(stderr, "RESET\n");
					break;

				case INST_SYNC_WRITE:
					fprintf(stderr, "SYNC_WRITE\n");
					break;

				case INST_BULK_READ:
					fprintf(stderr, "BULK_READ\n");
					break;
                		case INST_SYNC_READ:
                    			fprintf(stderr, "SYNC_READ\n");
                    			break;
				default:
					fprintf(stderr, "UNKNOWN\n");
					break;
				}
		}

	if (length < (MAXNUM_TXPARAM + 6))
			{
                bool isBulk = false;
                bool isSync = false; 

			    m_Platform->ClearPort();

                if (txpacket[INSTRUCTION] == INST_BULK_READ ) 
                {
                        res = TxRxBulkPacket(txpacket, rxpacket, 1);
	                isBulk = true;
                } 
                if (txpacket[INSTRUCTION] == INST_SYNC_READ ) 
                {
                        res = TxRxSyncPacket(txpacket, rxpacket, 1);
	                isSync = true;
                } 
                if (!isBulk && !isSync)
	  	       		{
		            	if (m_Platform->WritePort(txpacket, length) == length)
							{	
								if (txpacket[ID] != ID_BROADCAST)
									{
										int to_length = 0;

										if (txpacket[INSTRUCTION] == INST_READ)
											to_length = txpacket[PARAMETER + 1] + 6;
										else
											to_length = 6;

										m_Platform->FlushPort();
										m_Platform->SetPacketTimeout(length);

										int get_length = 0;
										if (DEBUG_PRINT == true)
											fprintf(stderr, "RX: ");

										while (1)
											{
												length = m_Platform->ReadPort(&rxpacket[get_length], to_length - get_length);
												if (DEBUG_PRINT == true)
													{
														for (int n = 0; n < length; n++)
															fprintf(stderr, "%.2X ", rxpacket[get_length + n]);
													}
												get_length += length;

												if (get_length == to_length)
														{
															// Find packet header
															int i;
															for (i = 0; i < (get_length - 1); i++)
																{
																	if (rxpacket[i] == 0xFF && rxpacket[i + 1] == 0xFF)
																		break;
																	else if (i == (get_length - 2) && rxpacket[get_length - 1] == 0xFF)
																		break;
																}

															if (i == 0)
																{
																	// Check checksum
																	unsigned char checksum = CalculateChecksum(rxpacket);
																	if (DEBUG_PRINT == true)
																		fprintf(stderr, "CHK:%.2X\n", checksum);

																	if (rxpacket[get_length - 1] == checksum)
																		res = SUCCESS;
																	else
																		res = RX_CORRUPT;

																	break;
																}
															else
																{
																	for (int j = 0; j < (get_length - i); j++)
																		rxpacket[j] = rxpacket[j + i];
																	get_length -= i;
																}
														}
													else
														{
															if (m_Platform->IsPacketTimeout() == true)
																{
																	if (get_length == 0)
																		res = RX_TIMEOUT;
																	else
																		res = RX_CORRUPT;

																	break;
																}
														}
											}
									}
								else
									res = SUCCESS;
							}
						else
							res = TX_FAIL;
					}
			}
	else
		res = TX_CORRUPT;

	if (DEBUG_PRINT == true)
		{
			fprintf(stderr, "Time:%.2fms  ", m_Platform->GetPacketTime());
			fprintf(stderr, "RETURN: ");
			switch (res)
				{
				case SUCCESS:
					fprintf(stderr, "SUCCESS\n");
					break;

				case TX_CORRUPT:
					fprintf(stderr, "TX_CORRUPT\n");
					break;

				case TX_FAIL:
					fprintf(stderr, "TX_FAIL\n");
					break;

				case RX_FAIL:
					fprintf(stderr, "RX_FAIL\n");
					break;

				case RX_TIMEOUT:
					fprintf(stderr, "RX_TIMEOUT\n");
					break;

				case RX_CORRUPT:
					fprintf(stderr, "RX_CORRUPT\n");
					break;

				default:
					fprintf(stderr, "UNKNOWN\n");
					break;
				}
		}

	m_Platform->HighPriorityRelease();
	if (priority > 0)
		m_Platform->MidPriorityRelease();
	if (priority > 1)
		m_Platform->LowPriorityRelease();

	return res;
}

unsigned char ArbotixPro::CalculateChecksum(unsigned char *packet)
{
	unsigned char checksum = 0x00;
	for (int i = 2; i < packet[LENGTH] + 3; i++ )
		checksum += packet[i];
	return (~checksum);
}


//FOR AX servos
void ArbotixPro::MakeSyncReadPacket(int address, int length)
{
        int number = 0;

		if (m_bIncludeTempData == true){
				m_SyncReadTxPacket[ID]              = (unsigned char)0xFD;
				m_SyncReadTxPacket[INSTRUCTION]     = INST_SYNC_READ;
				{
						m_SyncReadTxPacket[PARAMETER ] = AXDXL::P_PRESENT_TEMPERATURE; // start address
						m_SyncReadTxPacket[PARAMETER + 1] = 1;
				}
				for (int id = JointData::ID_MIN; id <= JointData::ID_MAX; id++)
					{
						if (MotionStatus::m_CurrentJoints.GetEnable(id))
							{
								m_SyncReadTxPacket[PARAMETER + number + 2] = id; // id
								number++;
							}
					}

				/**for(int id = 1; id < JointData::NUMBER_OF_JOINTS; id++)
				{
					if(MotionStatus::m_CurrentJoints.GetEnable(id))
					{
						m_BulkReadTxPacket[PARAMETER+3*number+1] = 2;   // length
						m_BulkReadTxPacket[PARAMETER+3*number+2] = id;  // id
						m_BulkReadTxPacket[PARAMETER+3*number+3] = AXDXL::P_PRESENT_POSITION_L; // start address
						number++;
					}
				}*/

				//m_SyncReadTxPacket[PARAMETER + 2] = ArbotixPro::ID_CM; // id
				//number++;
				//fprintf(stderr, "NUMBER : %d \n", number);

				m_SyncReadTxPacket[LENGTH]          = (number) + 4;
		}		
}

//FOR other devices in Dynamixel bus.
void ArbotixPro::MakeBulkReadPacket()
{
	int number = 0;

        m_BulkReadTxPacket[ID]              = (unsigned char)ID_BROADCAST;
        m_BulkReadTxPacket[INSTRUCTION]     = INST_BULK_READ;
        m_BulkReadTxPacket[PARAMETER]       = (unsigned char)0x0;

        //if(Ping(ArbotixPro::ID_CM, 0) == SUCCESS)
        {
                m_BulkReadTxPacket[PARAMETER + 3 * number + 1] = 30;
                m_BulkReadTxPacket[PARAMETER + 3 * number + 2] = ArbotixPro::ID_CM;
                m_BulkReadTxPacket[PARAMETER + 3 * number + 3] = ArbotixPro::P_DXL_POWER;
                number++;
        }
        /*
        if(Ping(FSR::ID_L_FSR, 0) == SUCCESS)
        {
        m_BulkReadTxPacket[PARAMETER+3*number+1] = 2;               // length
        m_BulkReadTxPacket[PARAMETER+3*number+2] = FSR::ID_L_FSR;   // id
        m_BulkReadTxPacket[PARAMETER+3*number+3] = FSR::P_FSR_X;    // start address
        number++;
        }

        if(Ping(FSR::ID_R_FSR, 0) == SUCCESS)
        {
        m_BulkReadTxPacket[PARAMETER+3*number+1] = 2;               // length
        m_BulkReadTxPacket[PARAMETER+3*number+2] = FSR::ID_R_FSR;   // id
        m_BulkReadTxPacket[PARAMETER+3*number+3] = FSR::P_FSR_X;    // start address
        number++;
        }
        */
        //fprintf(stderr, "NUMBER : %d \n", number);
        m_BulkReadTxPacket[LENGTH]          = (number * 3) + 3;
        /**int length = m_BulkReadTxPacket[LENGTH] + 4;
        fprintf(stderr, "\nTX: ");
        for (int n = 0; n < length; n++)
              fprintf(stderr, "%.2X ",  m_BulkReadTxPacket[n]);
        */ 

}

int ArbotixPro::SyncRead()
{
        unsigned char rxpacket[MAXNUM_RXPARAM + 10] = {0, };

        if (m_SyncReadTxPacket[LENGTH] != 0)
           return TxRxPacket(m_SyncReadTxPacket, rxpacket, 0,true);
        else
           {
                MakeSyncReadPacket(0,0);
                return TX_FAIL;
           }       
}

int ArbotixPro::BulkRead()
{
	unsigned char rxpacket[MAXNUM_RXPARAM + 10] = {0, };
	if (m_BulkReadTxPacket[LENGTH] != 0)
	{
		SyncRead();
		return TxRxPacket(m_BulkReadTxPacket, rxpacket, 0, true);
	}
	else
	{
		MakeBulkReadPacket();
		MakeSyncReadPacket(0,0);
		return TX_FAIL;
	}
}

int ArbotixPro::SyncWrite(int start_addr, int each_length, int number, int *pParam)
{
	unsigned char txpacket[MAXNUM_TXPARAM + 10] = {0, };
	unsigned char rxpacket[MAXNUM_RXPARAM + 10] = {0, };
	int n;

	txpacket[ID]                = (unsigned char)ID_BROADCAST;
	txpacket[INSTRUCTION]       = INST_SYNC_WRITE;
	txpacket[PARAMETER]			= (unsigned char)start_addr;
	txpacket[PARAMETER + 1]		= (unsigned char)(each_length - 1);
	for (n = 0; n < (number * each_length); n++)
		txpacket[PARAMETER + 2 + n]   = (unsigned char)pParam[n];
	txpacket[LENGTH]            = n + 4;

	return TxRxPacket(txpacket, rxpacket, 0,false);
}

bool ArbotixPro::Connect()
{
	if (m_Platform->OpenPort() == false)
		{
			fprintf(stderr, "\n Fail to open port\n");
			fprintf(stderr, " Arbotix Pro is used by another program or do not have root privileges.\n\n");
			return false;
		}

	//return DXLPowerOn();
        return true;
}

bool ArbotixPro::ChangeBaud(int baud)
{
	if (m_Platform->SetBaud(baud) == false)
		{
			fprintf(stderr, "\n Fail to change baudrate\n");
			return false;
		}

	//return DXLPowerOn();
        return true;
}

bool ArbotixPro::DXLPowerOn(bool state)
{
	if (WriteByte(ArbotixPro::ID_CM, ArbotixPro::P_DXL_POWER, state == true ? 1 : 0, 0) == ArbotixPro::SUCCESS)
		{
			if (DEBUG_PRINT == true)
				fprintf(stderr, " Succeed to change Dynamixel power!\n");

			m_Platform->Sleep(300); // about 300msec
		}
	else
		{
			if (DEBUG_PRINT == true)
				fprintf(stderr, " Fail to change Dynamixel power!\n");
			return false;
		}

	return true;
}

void ArbotixPro::Disconnect()
{
// do action upon disconnect
	unsigned char txpacket[] = {0xFF, 0xFF, 0xC8, 0x05, 0x03, 0x1A, 0xE0, 0x03, 0x32};
	m_Platform->WritePort(txpacket, 9);

	m_Platform->ClosePort();
}

int ArbotixPro::WriteByte(int address, int value, int *error)
{
	return WriteByte(ID_CM, address, value, error);
}

int ArbotixPro::WriteWord(int address, int value, int *error)
{
	return WriteWord(ID_CM, address, value, error);
}

void ArbotixPro::WriteWordDelayed(int address, int value)
{
	if (m_DelayedWords > 9) return;

	m_DelayedWord[m_DelayedWords] = value;
	m_DelayedAddress[m_DelayedWords] = address;
	m_DelayedWords++;
	return;
}

int ArbotixPro::Ping(int id, int *error)
{
	unsigned char txpacket[MAXNUM_TXPARAM + 10] = {0, };
	unsigned char rxpacket[MAXNUM_RXPARAM + 10] = {0, };
	int result;

	txpacket[ID]           = (unsigned char)id;
	txpacket[INSTRUCTION]  = INST_PING;
	txpacket[LENGTH]       = 2;

	result = TxRxPacket(txpacket, rxpacket, 2,false);
	if (result == SUCCESS && txpacket[ID] != ID_BROADCAST)
		{
			if (error != 0)
				*error = (int)rxpacket[ERRBIT];
		}

	return result;
}

int ArbotixPro::RebootUsb2Ax(int *error)
{
        unsigned char txpacket[MAXNUM_TXPARAM + 10] = {0, };
        unsigned char rxpacket[MAXNUM_RXPARAM + 10] = {0, };
        int result;

        txpacket[ID]           = (unsigned char)253;
        txpacket[INSTRUCTION]  = INST_RESET;
        txpacket[LENGTH]       = 2;

        result = TxRxPacket(txpacket, rxpacket, 2,false);
        if (result == SUCCESS && txpacket[ID] != ID_BROADCAST)
                {
                        if (error != 0)
                                *error = (int)rxpacket[ERRBIT];
                }

        return result;
}

int ArbotixPro::ReadByte(int id, int address, int *pValue, int *error)
{
	unsigned char txpacket[MAXNUM_TXPARAM + 10] = {0, };
	unsigned char rxpacket[MAXNUM_RXPARAM + 10] = {0, };
	int result;

	txpacket[ID]           = (unsigned char)id;
	txpacket[INSTRUCTION]  = INST_READ;
	txpacket[PARAMETER]    = (unsigned char)address;
	txpacket[PARAMETER + 1]  = 1;
	txpacket[LENGTH]       = 4;

	result = TxRxPacket(txpacket, rxpacket, 2,false);
	if (result == SUCCESS)
		{
			*pValue = (int)rxpacket[PARAMETER];
			if (error != 0)
				*error = (int)rxpacket[ERRBIT];
		}

	return result;
}

int ArbotixPro::ReadWord(int id, int address, int *pValue, int *error)
{
	unsigned char txpacket[MAXNUM_TXPARAM + 10] = {0, };
	unsigned char rxpacket[MAXNUM_RXPARAM + 10] = {0, };
	int result;

	txpacket[ID]           = (unsigned char)id;
	txpacket[INSTRUCTION]  = INST_READ;
	txpacket[PARAMETER]    = (unsigned char)address;
	txpacket[PARAMETER + 1]  = 2;
	txpacket[LENGTH]       = 4;

	result = TxRxPacket(txpacket, rxpacket, 2, false);
	if (result == SUCCESS)
		{
			*pValue = MakeWord((int)rxpacket[PARAMETER], (int)rxpacket[PARAMETER + 1]);

			if (error != 0)
				*error = (int)rxpacket[ERRBIT];
		}

	return result;
}

int ArbotixPro::ReadTable(int id, int start_addr, int end_addr, unsigned char *table, int *error)
{
	unsigned char txpacket[MAXNUM_TXPARAM + 10] = {0, };
	unsigned char rxpacket[MAXNUM_RXPARAM + 10] = {0, };
	int result;
	int length = end_addr - start_addr + 1;

	txpacket[ID]           = (unsigned char)id;
	txpacket[INSTRUCTION]  = INST_READ;
	txpacket[PARAMETER]    = (unsigned char)start_addr;
	txpacket[PARAMETER + 1]  = (unsigned char)length;
	txpacket[LENGTH]       = 4;

	result = TxRxPacket(txpacket, rxpacket, 1, false);
	if (result == SUCCESS)
		{
			for (int i = 0; i < length; i++)
				table[start_addr + i] = rxpacket[PARAMETER + i];

			if (error != 0)
				*error = (int)rxpacket[ERRBIT];
		}

	return result;
}

int ArbotixPro::WriteByte(int id, int address, int value, int *error)
{
	unsigned char txpacket[MAXNUM_TXPARAM + 10] = {0, };
	unsigned char rxpacket[MAXNUM_RXPARAM + 10] = {0, };
	int result;

	txpacket[ID]           = (unsigned char)id;
	txpacket[INSTRUCTION]  = INST_WRITE;
	txpacket[PARAMETER]    = (unsigned char)address;
	txpacket[PARAMETER + 1]  = (unsigned char)value;
	txpacket[LENGTH]       = 4;

	result = TxRxPacket(txpacket, rxpacket, 2,false);
	if (result == SUCCESS && id != ID_BROADCAST)
		{
			if (error != 0)
				*error = (int)rxpacket[ERRBIT];
		}

	return result;
}

int ArbotixPro::WriteWord(int id, int address, int value, int *error)
{
	unsigned char txpacket[MAXNUM_TXPARAM + 10] = {0, };
	unsigned char rxpacket[MAXNUM_RXPARAM + 10] = {0, };
	int result;

	txpacket[ID]           = (unsigned char)id;
	txpacket[INSTRUCTION]  = INST_WRITE;
	txpacket[PARAMETER]    = (unsigned char)address;
	txpacket[PARAMETER + 1]  = (unsigned char)GetLowByte(value);
	txpacket[PARAMETER + 2]  = (unsigned char)GetHighByte(value);
	txpacket[LENGTH]       = 5;

	result = TxRxPacket(txpacket, rxpacket, 2, false);
	if (result == SUCCESS && id != ID_BROADCAST)
		{
			if (error != 0)
				*error = (int)rxpacket[ERRBIT];
		}

	return result;
}

int ArbotixPro::MakeWord(int lowbyte, int highbyte)
{
	unsigned short word;

	word = highbyte;
	word = word << 8;
	word = word + lowbyte;

	return (int)word;
}

int ArbotixPro::GetLowByte(int word)
{
	unsigned short temp;
	temp = word & 0xff;
	return (int)temp;
}

int ArbotixPro::GetHighByte(int word)
{
	unsigned short temp;
	temp = word & 0xff00;
	return (int)(temp >> 8);
}
