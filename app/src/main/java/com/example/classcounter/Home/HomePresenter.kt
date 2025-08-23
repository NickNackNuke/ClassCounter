package com.example.classcounter

class HomePresenter : HomeContract.Presenter {
    private var view: HomeContract.View? = null

    override fun attach(view: HomeContract.View) { this.view = view }
    override fun detach() { view = null }

    override fun load() {
        view?.showStats("24", "56", "32")
        view?.showPeopleInside("24")
        view?.showLastUpdated("Updated 2m ago")
    }

    override fun onViewActivityLogClicked() {
        view?.showMessage("Activity Log clicked")
    }
}
