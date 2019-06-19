#include <vector>
#include <string>
#include <exception>
#include <algorithm>

#include "order_controller.h"

#include <ros/ros.h>

OrderController::OrderController(const std::string& path) 
  : m_msgpath(path)
  , m_connected(false)
  , m_connection(nullptr)
{
  initConnectionObj();
}


void OrderController::initConnectionObj() {
  std::vector<std::string> paths = {m_msgpath};
  m_connection = std::make_unique<protobuf_comm::ProtobufStreamClient>(paths); 
  m_connection->signal_connected().connect(
		  boost::bind(&OrderController::connected, boost::ref(*this))
		  );
  m_connection->signal_disconnected().connect(
		  boost::bind(&OrderController::disconnected, boost::ref(*this), boost::placeholders::_1)
	          );
}

void OrderController::convertOrderInfo(const ros_opencart::Order& in_order, llsf_msgs::OrderInfo& out_orders) {
  
  for(const ros_opencart::Item& item : in_order.items) {
    llsf_msgs::Order* orderlist = out_orders.add_orders();
    llsf_msgs::Order order;
  
    //-- set complexity 
    int complexity = item.model[1]-'0';
    switch(complexity) {
      case 0:
        order.set_complexity(llsf_msgs::Order_Complexity::Order_Complexity_C0);
        break;
      case 1:
        order.set_complexity(llsf_msgs::Order_Complexity::Order_Complexity_C1);
        break;
      case 2:
        order.set_complexity(llsf_msgs::Order_Complexity::Order_Complexity_C2);
        break;
      case 3:
        order.set_complexity(llsf_msgs::Order_Complexity::Order_Complexity_C3);
        break;
    };

    //-- determine component colors
    llsf_msgs::BaseColor base_color;
    llsf_msgs::CapColor cap_color;
    std::vector<llsf_msgs::RingColor> ring_colors(complexity);

    for(const ros_opencart::Option& option : item.options) {
      std::string name = option.name;
      std::transform(name.begin(), name.end(), name.begin(), ::tolower);
      
      std::string value = option.value;
      std::transform(value.begin(), value.end(), value.begin(), ::tolower);

      if(!name.substr(0,5).compare("base_")) {
	if(!value.compare("red"))
	  base_color = llsf_msgs::BaseColor::BASE_RED;
	else if(!value.compare("black"))
	  base_color = llsf_msgs::BaseColor::BASE_BLACK;
	else  
  	  base_color = llsf_msgs::BaseColor::BASE_SILVER;

	order.set_base_color(base_color);

      } else if(!name.substr(0,4).compare("cap_")) {
	if(!value.compare("black"))
	  cap_color = llsf_msgs::CapColor::CAP_BLACK;
	else
	  cap_color = llsf_msgs::CapColor::CAP_GREY;
	
	order.set_cap_color(cap_color);
      
      } else if(!name.substr(0,5).compare("ring_")) {
	int ring_idx = name[name.length()-1]-'0'-1;

	if(!value.compare("blue"))
	  ring_colors[ring_idx] = llsf_msgs::RingColor::RING_BLUE;
	else if(!value.compare("green"))
	  ring_colors[ring_idx] = llsf_msgs::RingColor::RING_GREEN;
	else if(!value.compare("orange"))
	  ring_colors[ring_idx] = llsf_msgs::RingColor::RING_ORANGE;
	else  
  	  ring_colors[ring_idx] = llsf_msgs::RingColor::RING_YELLOW;
      }
    }
  
    //-- apply ring colors
    for(llsf_msgs::RingColor rc : ring_colors)
      order.add_ring_colors(rc);


    
    //-- this is ignored by refbox, anyways
    order.set_id(1337);
    order.set_delivery_gate(1);
    order.set_delivery_period_begin(1);
    order.set_delivery_period_end(2);
    order.set_quantity_delivered_cyan(0);
    order.set_quantity_delivered_magenta(0);
    order.set_competitive(false);

    order.set_quantity_requested(1);

    orderlist[0] = order;
  }
}

void OrderController::connected() {
  m_connected = true;
}

void OrderController::disconnected(const boost::system::error_code& err) {
  m_connected = false;
  if(!(!err))
    throw std::runtime_error(err.message());
}


void OrderController::setRefboxHost(const std::string& host) {
  m_refboxhost = host;
}
const std::string& OrderController::getRefboxHost() {
  return m_refboxhost;
}

void OrderController::setPort(uint32_t port) {
  m_port = port;
}
uint32_t OrderController::getPort() {
 return m_port;
}

bool OrderController::isConnected() {
  return m_connected;
}



void OrderController::connect() {
  if(m_connected)
    return;

  m_connection->async_connect(m_refboxhost.c_str(), m_port);
}
void OrderController::disconnect() {
  if(!m_connected)
    return;

  m_connection->disconnect();
}

bool OrderController::sendOrder(const ros_opencart::Order& in_order) {
  if(!m_connected)
    return false;

  llsf_msgs::OrderInfo snd_order;

  try {
    convertOrderInfo(in_order, snd_order);
    m_connection->send(snd_order.COMP_ID, snd_order.MSG_TYPE, snd_order);

  } catch (std::runtime_error& ex) {
    throw ex;
  }

  return true;
}

