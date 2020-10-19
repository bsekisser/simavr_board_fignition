typedef struct fignition_spi_t** fignition_spi_pp;
typedef struct fignition_spi_t* fignition_spi_p;

void fignition_spi_connect(fignition_spi_p spi);
int fignition_spi_init(struct avr_t* avr, fignition_spi_pp p);
