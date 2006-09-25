int ext_lock_dev(void *);
void ext_unlock_dev(void *);
struct rt_ec_dev *register_ec_master(unsigned int);
struct rt_ec_domain *register_ec_domain(struct rt_ec_dev *,unsigned int);
void ethercat_support_free_master(struct rt_ec_dev *);
void ethercat_support_lock_master(struct rt_ec_dev *);
void ethercat_support_unlock_master(struct rt_ec_dev *);
